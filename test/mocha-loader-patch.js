mocha = require("mocha")
// stash a reference away to this
old_loader = mocha.prototype.loadfiles

if (!old_loader.monkey_patched) {
  // gee thanks mocha ...
  mocha.prototype.loadfiles = function() {
    try {
      old_loader.apply(this, arguments);
    } catch (e) {
      // normally mocha just silently bails
      console.error(e.stack);
      // we throw, so mocha doesn't continue trying to run the test suite
      throw e;
    }
  }
  mocha.prototype.loadfiles.monkey_patched = true;
};

