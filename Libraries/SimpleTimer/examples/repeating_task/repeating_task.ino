// repeating_task.ino
#include <SimpleTimer.h>
#include <PString.h>

// instantiate timer object
SimpleTimer timer = SimpleTimer();

// instantiate string object
char buffer[14];
PString str(buffer, sizeof(buffer));

int mins = 0, secs = 0;

// periodic function
void tick() {
  ++secs;
  if (secs == 60) {
    secs = 0;
    ++mins;
  }
  str = "";
  str.print(str.format("Uptime: %02d:%02d", mins, secs));
  Serial.println(str);
} // tick

void setup() {
  Serial.begin(9600);
  timer.setInterval(1000, tick);
}

void loop() {
  timer.run();
}


