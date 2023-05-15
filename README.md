# voyager-media

exploring to add light weight library for space media player base on ffmpeg. thus, we can compile the leight weight library to wasm format. 
which will improve to support wide range of media format, media codec and professional media filter effects in the web browser. in addition to this,
we still have the flexible of tuning the performance of the light weight library.

### git large file

1. install git large file for your os.

2. configure git large file

```bash
git lfs install

git lfs track "*.a"

git add .gitatrributes
```

3. configure your git for `GnuTLS recv error (-110): The TLS connection was non-properly terminated`

```bash
git config --global http.postBuffer 1048576000
```

### Streams

* rtmp

广西卫视：rtmp://58.200.131.2:1935/livetv/gxtv

湖南卫视：rtmp://58.200.131.2:1935/livetv/hunantv

* rtsp

一段动画 rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov

* flv

一段录播 http://1011.hlsplay.aodianyun.com/demo/game.flv

西瓜播放器测试视频 https://sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/flv/xgplayer-demo-360p.flv

超清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/flv/xgplayer-demo-720p.flv 

高清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/flv/xgplayer-demo-480p.flv

标清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/flv/xgplayer-demo-360p.flv

* MP4

依然是西瓜 https://sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/mp4/xgplayer-demo-360p.mp4

超清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/mp4/xgplayer-demo-720p.mp4

高清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/mp4/xgplayer-demo-480p.mp4

标清 //sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/mp4/xgplayer-demo-360p.mp4

* hls

西瓜播放器测试 https://sf1-hscdn-tos.pstatp.com/obj/media-fe/xgplayer_doc_video/hls/xgplayer-demo.m3u8