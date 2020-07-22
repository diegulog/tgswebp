#include <iostream>
#include <src/webp/encode.h>
#include <src/webp/mux.h>
#include <imageio/imageio_util.h>
#include "lib/rlottie/inc/rlottie.h"

using namespace std;

template <typename T>
void printer(T t)
{
    cout << t << endl;
}

template <typename T, typename ...U>
void printer(T t, U ...u)
{
    cout << t;
    printer(u...);
}

int main(int argc, char* argv[]) {

    const char * in_file = nullptr, *out_file = nullptr;

    in_file = argv[1];
    out_file = argv[2];

    printer("file in: ",in_file);
    printer("file out: ", out_file);

    auto player = rlottie::Animation::loadFromFile(in_file);

    int width = 512;
    int height = 512;
    auto buffer = std::unique_ptr<uint32_t[]>(new uint32_t[width * height]);
    int totalFrame = player->totalFrame();
    printer("totalFrame: ", totalFrame);
    double d = player->duration();
    printer("total duracion: ", d);
    WebPAnimEncoder *enc = nullptr;
    int pic_num = 0;
    int salto = 1;
    if( totalFrame > 20 ) salto = totalFrame/20;
    printer("Salto: ", salto);
    int duration = (d / (totalFrame / salto))*1000;
    printer("duration: ", duration);
    int timestamp_ms = 0;
    int loop_count = 0;
    WebPAnimEncoderOptions anim_config;
    WebPConfig config;
    WebPPicture pic;
    WebPData webp_data;
    int ok;

    WebPDataInit(&webp_data);
    if (!WebPAnimEncoderOptionsInit(&anim_config) ||
        !WebPConfigInit(&config) ||
        !WebPPictureInit(&pic)) {
        printf("Library version mismatch!");
        ok = 0;
        goto End;
    }
    pic_num = 0;
    pic.width = width;
    pic.height = height;
    config.method = 6;
    config.quality = 20;
    anim_config.allow_mixed = 1;
    config.lossless = 0;

    ok = WebPValidateConfig(&config);
    if (!ok) {
        printf("Invalid configuration");
        goto End;
    }
    pic.use_argb = 1;
    for (size_t i = 0; i < totalFrame; i += salto) {
        printer("frame: ", i);
        rlottie::Surface surface(buffer.get(), width, height, width * 4);
        player->renderSync(i, surface);
        if (!WebPPictureAlloc(&pic)) return 0;   // memory error
        pic.argb = (uint32_t*)surface.buffer();
        //  argbTorgba(surface);
        //  ok = WebPPictureImportRGBA(&pic, reinterpret_cast<uint8_t *>(surface.buffer()), height*4);
        /*  if (!ok) {
              printf("FAIL ReadImage");
          }*/
        if (enc == nullptr) {
            width = pic.width;
            height = pic.height;
            enc = WebPAnimEncoderNew(width, height, &anim_config);
            ok = (enc != nullptr);
            if (!ok) {
                printf("Could not create WebPAnimEncoder object.");
            }
        }

        if (ok) {
            ok = (width == pic.width && height == pic.height);
            if (!ok) {
                printf("Framedimension mismatched! ");

            }
        }

        if (ok) {
            ok = WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config);
            if (!ok) {
                printf("Error while adding frame");
            }
        }
        WebPPictureFree(&pic);
        timestamp_ms += duration;
        ++pic_num;
    }

    ok = ok && WebPAnimEncoderAdd(enc, nullptr, timestamp_ms, nullptr);
    ok = ok && WebPAnimEncoderAssemble(enc, &webp_data);
    if (!ok) {
        printf("Error during final animation assembly.");
    }

    End:
    WebPAnimEncoderDelete(enc);
    if (ok) {

        ok = ImgIoUtilWriteFile(out_file, webp_data.bytes, webp_data.size);
        if (ok) printf("output file: ");
    }
    WebPDataClear(&webp_data);
    printf("WebP rendering end ");


    return 0;
}




