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

#include <webp/mux.h>
#include <../imageio/imageio_util.h>
#include <webp/encode.h>
#include "lib/rlottie/inc/rlottie.h"
#include "../examples/unicode.h"
#include "../examples/example_util.h"
#include "zlib.h"

#define BUFLEN      16384
#ifdef MAXSEG_64K
#  define local static
/* Needed for systems with limitation on stack size. */
#else
#  define local
#endif
#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

//------------------------------------------------------------------------------

static char *prog;


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

void error(const char *msg) {
    fprintf(stderr, "%s: %s\n", prog, msg);
    exit(1);
}

std::string gz_uncompress(gzFile in) {
    local char buf[BUFLEN];
    std::string json;
    int len;
    int err;
    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) error(gzerror(in, &err));
        if (len == 0) break;
        std::string s(buf, len);
        json += s;
    }
    if (gzclose(in) != Z_OK) {
        error("failed close");
    }
    return json;
}

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
    printf("  -frames  ............... print only original frames, test only method\n");
    printf("  -v ..................... verbose\n");
    printf("\n");
}

//------------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
    int verbose = 0;
    int ok = 1;
    const W_CHAR *in_file = nullptr, *out_file = nullptr;
    int frame_timestamp = 0;
    int frame_duration = 0;
    int pic_num = 0;
    int test_frames_info = 0;
    int width = 512, height = 512;
    int skip = 1;
    int total_frame_lottie = 1;
    int duration_lottie = 0;
    std::unique_ptr<rlottie::Animation> player;
    std::unique_ptr<uint32_t[]> buffer;
    WebPPicture frame;                // Frame rectangle only (not disposed).
    WebPAnimEncoder *enc = nullptr;
    WebPAnimEncoderOptions enc_options;
    WebPConfig config;

    int c;
    WebPData webp_data;
    WebPMux *mux = nullptr;

    if (argc == 1) {
        Help();
        FREE_WARGV_AND_RETURN(1);
    }
    if (!WebPConfigInit(&config) || !WebPAnimEncoderOptionsInit(&enc_options) ||
        !WebPPictureInit(&frame)) {
        fprintf(stderr, "Error! Version mismatch!\n");
        ok = 0;
        goto End;
    }
    WebPDataInit(&webp_data);

    for (c = 1; ok && c < argc; ++c) {
        int parse_error = 0;
        if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
            Help();
            goto End;
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
            goto End;
        } else if (!strcmp(argv[c], "-frames")) {
            test_frames_info = 1;
        } else if (!strcmp(argv[c], "-v")) {
            verbose = 1;
        } else if (!strcmp(argv[c], "--")) {
            if (c < argc - 1) in_file = GET_WARGV(argv, ++c);
            break;
        } else if (argv[c][0] == '-') {
            fprintf(stderr, "Error! Unknown option '%s'\n", argv[c]);
            Help();
            goto End;
        } else {
            in_file = GET_WARGV(argv, c);
        }

        ok = !parse_error;
        if (!ok) goto End;
    }

    if (!enc_options.allow_mixed) config.lossless = 1;
    config.sns_strength = 90;
    config.filter_sharpness = 6;
    config.alpha_quality = 5;
    ok = WebPValidateConfig(&config);
    if (!ok) {
        fprintf(stderr, "Error! Invalid configuration.\n");
        goto End;
    }

    ok = in_file != nullptr;
    if (!ok) {
        fprintf(stderr, "No input file specified!\n");
        Help();
        goto End;
    }


    if (tgsFile(in_file)) {
        gzFile file = gzopen(in_file, "rb");
        if (file == nullptr) error("can't open tgs file");
        std::string json = gz_uncompress(file);
        if (!json.empty()) player = rlottie::Animation::loadFromData(json, in_file);
    } else if (jsonFile(in_file)) {
        player = rlottie::Animation::loadFromFile(in_file, true);
    } else {
        fprintf(stderr, "Invalid input file format, only supports json or tgs\n");
        Help();
        ok = 0;
        goto End;
    }

    // Start the decoder object
    player = rlottie::Animation::loadFromFile(in_file);

    ok = (player != nullptr);
    if (!ok) {
        fprintf(stderr, "Error init Animation ");
        goto End;
    }


    buffer = std::unique_ptr<uint32_t[]>(new uint32_t[width * height]);
    total_frame_lottie = player->totalFrame();
    duration_lottie = int(player->duration() * 1000);
    //  if( total_frame_lottie > 25 ) skip = total_frame_lottie/25;
    frame_duration = duration_lottie / (total_frame_lottie / skip);
    if(test_frames_info){
        printf( "%u\n", total_frame_lottie);
        goto End;
    }

    if (verbose) {
        fprintf(stderr, "Frames lottie:      %d\n", total_frame_lottie);
        fprintf(stderr, "Total duration:     %d ms\n", duration_lottie);
        fprintf(stderr, "Frame duration:     %d ms\n", frame_duration);
        fprintf(stderr, "Frames webp out:    %d\n", (total_frame_lottie / skip));
    }
    //  player->size(reinterpret_cast<size_t &>(width), reinterpret_cast<size_t &>(height));
    frame.width = width;
    frame.height = height;
    frame.use_argb = 1;

    for (int i = 0; i < total_frame_lottie; i += skip) {
        if (verbose) fprintf(stderr, "INFO: Added frame:  %d/%d \r", i, total_frame_lottie);
        rlottie::Surface surface(buffer.get(), width, height, width * 4);
        player->renderSync(i, surface);
        ok = WebPPictureAlloc(&frame);
        if (!ok) {
            goto End;
        }
        frame.argb = (uint32_t *) surface.buffer();

        if (enc == nullptr) {
            width = frame.width;
            height = frame.height;
            enc = WebPAnimEncoderNew(width, height, &enc_options);
            ok = (enc != nullptr);
            if (!ok) {
                fprintf(stderr, "Could not create WebPAnimEncoder object.");
            }
        }

        if (ok) {
            ok = (width == frame.width && height == frame.height);
            if (!ok) {
                fprintf(stderr, "Framedimension mismatched! ");
            }
        }

        if (ok) {
            ok = WebPAnimEncoderAdd(enc, &frame, frame_timestamp, &config);
            if (!ok) {
                fprintf(stderr, "Error while adding frame");
            }
        }

        /*    if (verbose) {
                WFPRINTF(stderr, "Added frame #%3d at time %4d (file: %s)\r",
                         pic_num, frame_timestamp, out_file);
            }*/
        WebPPictureFree(&frame);
        frame_timestamp += frame_duration;
        ++pic_num;
    }



    // Last NULL frame.
    ok = ok && WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL);
    ok = ok && WebPAnimEncoderAssemble(enc, &webp_data);
    if (!ok) {
        fprintf(stderr, "Error during final animation assembly.\n");
    }


    if (ok && out_file != nullptr) {
        ok = ImgIoUtilWriteFile(out_file, webp_data.bytes, webp_data.size);
        if (ok) WFPRINTF(stderr, "output file: %s  ", out_file);
    } else {
        fprintf(stderr, "Nothing written; use -o flag to save the result ");
    }
    if (ok) {
        fprintf(stderr, "[%d frames, %u bytes].\n",
                pic_num, (unsigned int) webp_data.size);
    }
    // All OK.
    End:
    WebPMuxDelete(mux);
    WebPDataClear(&webp_data);
    WebPPictureFree(&frame);
    WebPAnimEncoderDelete(enc);
    FREE_WARGV_AND_RETURN(!ok);
}

//------------------------------------------------------------------------------
