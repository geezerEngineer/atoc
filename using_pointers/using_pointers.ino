String s1, s2;
int t0;

void setup(void)
{
  Serial.begin(9600);
  s1 = "Line 1";
  s2 = "Line 2";
  t0 = millis();
}

void loop(void)
{
  tick();
}

void tick()
{
  if (millis() - t0 > 2000) {
    t0 = millis();
    wd(&s1, &s2);
  }
}


void wd(String* line1, String* line2)
{
  Serial.print(*line1);
  Serial.print("  ");
  Serial.println(*line2);
}



