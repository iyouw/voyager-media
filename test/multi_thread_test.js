const mod = require("../src/multi_thread.js");

mod().then((instance)=>{
  instance._open();
});