TIMEOUT(50000000); // 500 minutes


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
