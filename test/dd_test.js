const { appendFileSync, readSync, openSync } = require("fs");

const ffmpeg = require("../src/demux_decode.js");


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
  const ret = instance._open_dd(0, onOutputVideoFrameCallback, onOutputAudioFrameCallback);
  console.log(ret);

  // feed data
  
  const buffer = new Uint8Array(409600);
  let bytesRead = 0;
  let b;
  const onWriteDD = (opaque, pos, size) => {
    instance.writeArrayToMemory(b, pos);
  }
  const onWriteDDCallback = instance.addFunction(onWriteDD, 'viii');

  const fd = openSync(input_file);
  const feedData = () => {
    setTimeout(()=>{
      bytesRead = readSync(fd, buffer, 0, buffer.length);
      if (bytesRead == 0)
      {
        instance._write_is_done();
        return;
      }
      b = buffer.subarray(0, bytesRead);
      instance._write_dd(b.length, onWriteDDCallback);
      feedData();
    }, 100)
  }
  feedData();

  setInterval(()=>console.log('remain main thread'), 1000);
});