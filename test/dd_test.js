const { appendFileSync, readSync, openSync } = require("fs");

const ffmpeg = require("../src/demux_decode_w.js");


const input_file = "../data/xgplayer-demo-720p.mp4"
const video_output_file = "../result/xgplayer-demo-720p-video";
const audio_output_file = "../result/xgplayer-demo-720p-audio";

ffmpeg().then(async (instance)=>{
  // show hello
  instance._hello_wasm();

  // prepare callback
  // prepare demux && decode
  let vf = 0;
  let af = 0;
  const onOutputVideoFrame = (pos, size) => {
    console.log(`video_frames:${vf++},size:${size}`);
    const view = instance.HEAPU8;
    const buffer = view.subarray(pos, pos + size);
    appendFileSync(video_output_file, buffer);
  }

  const onOutputAudioFrame = (pos, size) => {
    console.log(`audio_frames:${af++},size:${size}`);
    const view = instance.HEAPU8;
    const buffer = view.subarray(pos, pos + size);
    appendFileSync(audio_output_file, buffer);
  }

  const onOutputVideoFrameCallback = instance.addFunction(onOutputVideoFrame, 'vii');
  const onOutputAudioFrameCallback = instance.addFunction(onOutputAudioFrame, 'vii');

  // open demux_decode
  const ret = instance._open_dd(onOutputVideoFrameCallback, onOutputAudioFrameCallback);
  console.log(ret);

  // feed data
  const buffer = new Uint8Array(4096);
  let bytesRead = 0;
  const fd = openSync(input_file);
  const feedData = () => {
    setTimeout(()=>{
      bytesRead = readSync(fd, buffer, 0, buffer.length);
      if (bytesRead == 0)
      {
        return;
      }
      const b = buffer.subarray(0, bytesRead);
      const pos = instance._ensure_write_data_size_of_dd(bytesRead);
      instance.writeArrayToMemory(b, pos);
      instance._did_write_data_to_dd(b.length);
      feedData();
    }, 1)
  }
  feedData();
});