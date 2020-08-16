//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  simple tool to convert animated lottie to WebP
//
// Authors: Diego Gl (diegulog@gmail.com)

#include <iostream>

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif

#if defined(HAVE_UNISTD_H) && HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <src/webp/encode.h>
#include <src/webp/mux.h>
#include <imageio/imageio_util.h>
#include "lib/rlottie/inc/rlottie.h"
#include "../examples/unicode.h"
#include "../examples/example_util.h"
#include <fstream>      // std::ifstream, std::ofstream
#include <zlib.h>

#define CHUNK 16384

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

//------------------------------------------------------------------------------

static const char *const kErrorMessages[-WEBP_MUX_NOT_ENOUGH_DATA + 1] = {
        "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
        "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"
};

static const char *ErrorString(WebPMuxError err) {
    assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
    return kErrorMessages[-err];
}

std::string basename(const std::string &str) {
    return str.substr(str.find_last_of("/\\") + 1);
}

bool jsonFile(std::string fileName) {
    std::string extn = ".json";
    return !(fileName.size() <= extn.size() ||
             fileName.substr(fileName.size() - extn.size()) != extn);
}

bool tgsFile(std::string fileName) {
    std::string extn = ".tgs";
    return !(fileName.size() <= extn.size() ||
             fileName.substr(fileName.size() - extn.size()) != extn);
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int decompress() {
    FILE *source = WFOPEN("/Users/diegulog/1696/3.gz", "r");;
    FILE *dest = WFOPEN("/Users/diegulog/1696/3.json", "w+");;
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void) inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void) inflateEnd(&strm);
                    return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void) inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void) inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

void zerr(int ret) {
    fputs("zpipe: ", stderr);
    switch (ret) {
        case Z_ERRNO:
            if (ferror(stdin))
                fputs("error reading stdin\n", stderr);
            if (ferror(stdout))
                fputs("error writing stdout\n", stderr);
            break;
        case Z_STREAM_ERROR:
            fputs("invalid compression level\n", stderr);
            break;
        case Z_DATA_ERROR:
            fputs("invalid or incomplete deflate data\n", stderr);
            break;
        case Z_MEM_ERROR:
            fputs("out of memory\n", stderr);
            break;
        case Z_VERSION_ERROR:
            fputs("zlib version mismatch!\n", stderr);
    }
}

enum {
    METADATA_ICC = (1 << 0),
    METADATA_XMP = (1 << 1),
    METADATA_ALL = METADATA_ICC | METADATA_XMP
};

//------------------------------------------------------------------------------

static void Help(void) {
    printf("Usage:\n");
    printf(" tgswebp [options] lottie_file -o webp_file\n");
    printf("Options:\n");
    printf("  -h / -help ............. this help\n");
    printf("  -lossy ................. encode image using lossy compression\n");
    printf("  -mixed ................. for each frame in the image, pick lossy\n"
           "                           or lossless compression heuristically\n");
    printf("  -q <float> ............. quality factor (0:small..100:big)\n");
    printf("  -m <int> ............... compression method (0=fast, 6=slowest)\n");
    printf("  -s <int> ............... skip frames to reduce the output size \n");
    printf("  -min_size .............. minimize output size (default:off)\n"
           "                           lossless compression by default; can be\n"
           "                           combined with -q, -m, -lossy or -mixed\n"
           "                           options\n");
    printf("  -f <int> ............... filter strength (0=off..100)\n");
    printf("  -mt .................... use multi-threading if available\n");
    printf("\n");
    printf("  -version ............... print version number and exit\n");
    printf("  -v ..................... verbose\n");
    printf("  -quiet ................. don't print anything\n");
    printf("\n");
}

//------------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
    int verbose = 0;
    WebPMuxError err = WEBP_MUX_OK;
    int ok = 0;
    const W_CHAR *in_file = NULL, *out_file = NULL;
    int frame_timestamp = 0;
    int frame_duration = 0;
    int width = 512, height = 512;
    int skip = 1;
    int total_frame_lottie = 1;
    double duration_lottie = 0;
    std::unique_ptr<rlottie::Animation> player;
    std::unique_ptr<uint32_t[]> buffer;
    WebPPicture frame;                // Frame rectangle only (not disposed).
    WebPAnimEncoder *enc = NULL;
    WebPAnimEncoderOptions enc_options;
    WebPConfig config;
    int c;
    int quiet = 0;
    WebPData webp_data;
    WebPMux *mux = NULL;

    if (!WebPConfigInit(&config) || !WebPAnimEncoderOptionsInit(&enc_options) ||
        !WebPPictureInit(&frame)) {
        fprintf(stderr, "Error! Version mismatch!\n");
        FREE_WARGV_AND_RETURN(-1);
    }
    WebPDataInit(&webp_data);

    if (argc == 1) {
        Help();
        FREE_WARGV_AND_RETURN(0);
    }

    for (c = 1; c < argc; ++c) {
        int parse_error = 0;
        if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
            Help();
            FREE_WARGV_AND_RETURN(0);
        } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
            out_file = GET_WARGV(argv, ++c);
        } else if (!strcmp(argv[c], "-lossy")) {
            config.lossless = 0;
        } else if (!strcmp(argv[c], "-mixed")) {
            enc_options.allow_mixed = 1;
            config.lossless = 0;
        } else if (!strcmp(argv[c], "-s") && c < argc - 1) {
            skip = ExUtilGetInt(argv[++c], 0, &parse_error);
        } else if (!strcmp(argv[c], "-q") && c < argc - 1) {
            config.quality = ExUtilGetFloat(argv[++c], &parse_error);
        } else if (!strcmp(argv[c], "-m") && c < argc - 1) {
            config.method = ExUtilGetInt(argv[++c], 0, &parse_error);
        } else if (!strcmp(argv[c], "-min_size")) {
            enc_options.minimize_size = 1;
        } else if (!strcmp(argv[c], "-f") && c < argc - 1) {
            config.filter_strength = ExUtilGetInt(argv[++c], 0, &parse_error);
        } else if (!strcmp(argv[c], "-mt")) {
            ++config.thread_level;
        } else if (!strcmp(argv[c], "-version")) {
            const int enc_version = WebPGetEncoderVersion();
            const int mux_version = WebPGetMuxVersion();
            printf("WebP Encoder version: %d.%d.%d\nWebP Mux version: %d.%d.%d\n",
                   (enc_version >> 16) & 0xff, (enc_version >> 8) & 0xff,
                   enc_version & 0xff, (mux_version >> 16) & 0xff,
                   (mux_version >> 8) & 0xff, mux_version & 0xff);
            FREE_WARGV_AND_RETURN(0);
        } else if (!strcmp(argv[c], "-quiet")) {
            quiet = 1;
            enc_options.verbose = 0;
        } else if (!strcmp(argv[c], "-v")) {
            verbose = 1;
            enc_options.verbose = 1;
        } else if (!strcmp(argv[c], "--")) {
            if (c < argc - 1) in_file = GET_WARGV(argv, ++c);
            break;
        } else if (argv[c][0] == '-') {
            fprintf(stderr, "Error! Unknown option '%s'\n", argv[c]);
            Help();
            FREE_WARGV_AND_RETURN(-1);
        } else {
            in_file = GET_WARGV(argv, c);
        }

        if (parse_error) {
            Help();
            FREE_WARGV_AND_RETURN(-1);
        }
    }

    if (!enc_options.allow_mixed) config.lossless = 1;
    config.sns_strength = 100;
    config.filter_sharpness = 6;
    config.alpha_quality = 10;

    if (!WebPValidateConfig(&config)) {
        fprintf(stderr, "Error! Invalid configuration.\n");
        goto End;
    }

    if (in_file == NULL) {
        fprintf(stderr, "No input file specified!\n");
        Help();
        goto End;
    }


    if (tgsFile(in_file)) {
        ok = decompress();
        if (ok != Z_OK) {
            zerr(ok);
            Help();
            goto End;
        } else {
            player = rlottie::Animation::loadFromData(in_file, in_file);
        }
    } else if (jsonFile(in_file)) {
        player = rlottie::Animation::loadFromFile(in_file);
    } else {
        fprintf(stderr, "Invalid input file format, only supports json or tgs\n");
        Help();
        goto End;
    }

    // Start the decoder object
    player = rlottie::Animation::loadFromFile(in_file);
    buffer = std::unique_ptr<uint32_t[]>(new uint32_t[width * height]);
    total_frame_lottie = player->totalFrame();
    duration_lottie = player->duration();


    //  if( total_frame_lottie > 25 ) skip = total_frame_lottie/25;
    frame_duration = (duration_lottie / (total_frame_lottie / skip)) * 1000;
    fprintf(stderr,"total frames lottie: %d\n", total_frame_lottie);
    fprintf(stderr,"total duration: %d  ms\n", int(duration_lottie*1000));
    fprintf(stderr,"frame duration: %d  ms\n", frame_duration);
    fprintf(stderr,"total frames webp out: %d\n", (total_frame_lottie/skip)+1);


    //  player->size(reinterpret_cast<size_t &>(width), reinterpret_cast<size_t &>(height));
    frame.width = width;
    frame.height = height;
    frame.use_argb = 1;
    if (player == NULL) goto End;

    for (int i = 0; i < total_frame_lottie; i += skip) {
        fprintf(stderr, "INFO: Added frame: %d/%d \r", i, total_frame_lottie);
        rlottie::Surface surface(buffer.get(), width, height, width * 4);
        player->renderSync(i, surface);
        if (!WebPPictureAlloc(&frame)) {
            goto End;
        }
        frame.argb = (uint32_t *) surface.buffer();

        if (enc == nullptr) {
            width = frame.width;
            height = frame.height;
            enc = WebPAnimEncoderNew(width, height, &enc_options);
            ok = (enc != nullptr);
            if (!ok) {
                printf("Could not create WebPAnimEncoder object.");
            }
        }

        if (ok) {
            ok = (width == frame.width && height == frame.height);
            if (!ok) {
                printf("Framedimension mismatched! ");

            }
        }

        if (ok) {
            ok = WebPAnimEncoderAdd(enc, &frame, frame_timestamp, &config);
            if (!ok) {
                printf("Error while adding frame");
            }
        }
        WebPPictureFree(&frame);
        frame_timestamp += frame_duration;
    }



    // Last NULL frame.
    if (!WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL)) {
        fprintf(stderr, "Error flushing WebP muxer.\n");
        fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
    }

    if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
        fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
        goto End;
    }

    if (out_file != NULL) {
        if (!ImgIoUtilWriteFile((const char *) out_file, webp_data.bytes,
                                webp_data.size)) {
            WFPRINTF(stderr, "Error writing output file: %s\n", out_file);
            goto End;
        }
        if (!quiet) {
            if (!WSTRCMP(out_file, "-")) {
                fprintf(stderr, "Saved %d bytes to STDIO\n",
                        (int) webp_data.size);
            } else {
                WFPRINTF(stderr, "Saved output file (%d bytes): %s\n",
                         (int) webp_data.size, out_file);
            }
        }
    } else {
        if (!quiet) {
            fprintf(stderr, "Nothing written; use -o flag to save the result "
                            "(%d bytes).\n", (int) webp_data.size);
        }
    }

    // All OK.
    ok = 1;
    End:
    WebPMuxDelete(mux);
    WebPDataClear(&webp_data);
    WebPPictureFree(&frame);
    WebPAnimEncoderDelete(enc);
    FREE_WARGV_AND_RETURN(!ok);
}

//------------------------------------------------------------------------------
