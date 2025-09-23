TIMEOUT(100000000); // in ms


log.log("Starting COOJA logger\n");

timeout_function = function () {
  log.log("Script timed out.\n");
  log.testOK();
}

while (true) {
  if (msg) {
    log.log(time + " " + id + " " + msg + "\n");
  }

  YIELD();
}
