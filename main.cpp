#include "mj/yaml.hpp"

#include <stdio.h>
#include <stdlib.h>

void Print(const mj::YamlEvent& parserEvent)
{
}

void BeginParse(char* str)
{
  mj::YamlParser p;
  p.Malloc = malloc;
  p.Realloc = realloc;
  p.Free = free;
  p.Strdup = _strdup;

  //p.input    = str;

  mj::YamlEvent event          = {};
  mj::EYamlEventType eventType = mj::EYamlEventType::None;

  while (eventType != mj::EYamlEventType::StreamEnd)
  {
    bool success = p.Parse(event);
    if (success)
    {
      Print(event);
      eventType = event.type;
      // yaml_event_delete(&event);
    }
    else
    {
      // TODO: log error
      break;
    }
  }
}

int main()
{
  FILE* f = fopen("SampleScene.unity", "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char* string = (char*)malloc(fsize + 1);
  if (string)
  {
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = '\0';

    BeginParse(string);
    free(string);
  }
}
