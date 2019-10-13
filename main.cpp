#include "mj/yaml_parser.hpp"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

void Print(const mj::YamlParserEvent& parserEvent)
{
}

void BeginParse(char* str)
{
  mj::YamlParser p;
  p.pAllocFn = malloc;
  p.pFreeFn  = free;
  p.input    = str;

  mj::YamlParserEvent event      = {};
  mj::EYamlParserEventType eventType = mj::EYamlParserEventType::None;

  while (eventType != mj::EYamlParserEventType::StreamEnd)
  {
    bool success = p.Parse(event);
    if (success)
    {
      Print(event);
      eventType = event.type;
      //yaml_event_delete(&event);
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
