const { appendFileSync } = require("fs");
const { readFile } = require("fs/promises");
const ffmpeg = require("../src/trancode.js");


const input_file = "../data/xgplayer-demo-720p.mp4"
const video_output_file = "../result/xgplayer-demo-720p-video";
const audio_output_file = "../result/xgplayer-demo-720p-audio";

ffmpeg().then(async (instance)=>{
  let ret = 0;
  // show hello
  instance._hello_wasm();
  // prepare data
  instance._open_store();
  const inputData = await readFile(input_file);
  const buffer = new Uint8Array(inputData);
  const pos = instance._ensure_store_write_capacity(buffer.length);
  instance.writeArrayToMemory(buffer, pos);
  instance._did_write_store(buffer.length);
  let vf = 0;
  let af = 0;
  // prepare demux && decode
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

  const onStreamSelected = () => {
    ret = instance._open_decoder();
    console.log(`open decoder: ${ret}`);
  }

  const onPacketParsed = (streamIndex) => {
    ret = instance._decode(streamIndex, onOutputVideoFrameCallback, onOutputAudioFrameCallback);
  }

  const onStreamSelectedCallback = instance.addFunction(onStreamSelected, 'v');
  const onPacketParsedCallback = instance.addFunction(onPacketParsed, 'vi');

  ret = instance._open_demuxer(onStreamSelectedCallback, onPacketParsedCallback);
  console.log(ret);

  instance._close_store();
  instance._close_demuxer();
  instance._close_decoder();
});