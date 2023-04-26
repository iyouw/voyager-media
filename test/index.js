const { appendFileSync, readFileSync, readSync, openSync } = require("fs");
const { readFile, open } = require("fs/promises");
const ffmpeg = require("../src/trancode.js");


const input_file = "../data/xgplayer-demo-720p.mp4"
const video_output_file = "../result/xgplayer-demo-720p-video";
const audio_output_file = "../result/xgplayer-demo-720p-audio";

ffmpeg().then((instance)=>{
  let ret = 0;
  // show hello
  instance._hello_wasm();
  // prepare data
  // const inputData = await readFile(input_file);
  // const buffer = new Uint8Array(inputData);
  instance._open_store();
  const fd =  openSync(input_file);
  const rBuffer = new Uint8Array(1024);
  setTimeout(()=>{
    const bytesReads = readSync(fd, rBuffer);
    const b = rBuffer.subarray(0, bytesReads);
    const pos = instance._ensure_store_write_capacity(b.length);
    instance.writeArrayToMemory(b, pos);
    instance._did_write_store(b.length);
  }, 1000);
  // const pos = instance._ensure_store_write_capacity(buffer.length);
  // instance.writeArrayToMemory(buffer, pos);
  // instance._did_write_store(buffer.length);
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