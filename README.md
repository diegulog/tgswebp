          __   __  ____  ____  ____
         /  \\/  \/  _ \/  _ )/  _ \
         \       /   __/  _  \   __/
          \__\__/\____/\_____/__/ ____  ___
                / _/ /    \    \ /  _ \/ _/
               /  \_/   / /   \ \   __/  \__
               \____/____/\_____/_____/____/v1.1.0

## Description:

Library to encode lottie animation in WebP format.

## Building:
```shell script
mkdir build
cd ../build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF ..
cmake --build .
chmod +x tgswebp
```

## Animated lottie conversion:

```shell script
Usage:
 tgswebp [options] lottie_file -o webp_file
Options:
  -h / -help ............. this help
  -lossy ................. encode image using lossy compression
  -mixed ................. for each frame in the image, pick lossy
                           or lossless compression heuristically
  -s <int> ............... skip frames to reduce the output size
  -q <float> ............. quality factor (0:small..100:big)
  -m <int> ............... compression method (0=fast, 6=slowest)
  -f <int> ............... filter strength (0=off..100)
  -min_size .............. minimize output size (default:off)
                           lossless compression by default; can be
                           combined with -q, -m, -lossy or -mixed
                           options
  -mt .................... use multi-threading if available

  -version ............... print version number and exit
  -frames  ............... print only original frames, test only method
  -v ..................... verbose

```

## License

    Copyright 2023 Diego Guaña

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
