#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

struct Parser
{
  void* (*pAllocFn)(size_t) = nullptr;
  void (*pFreeFn)(void*) = nullptr;
  char* input = nullptr;
};

enum class EParserEventType
{
  YAML_NO_EVENT,
  YAML_STREAM_START_EVENT,
  YAML_STREAM_END_EVENT,
  YAML_DOCUMENT_START_EVENT,
  YAML_DOCUMENT_END_EVENT,
  YAML_ALIAS_EVENT,
  YAML_SCALAR_EVENT,
  YAML_SEQUENCE_START_EVENT,
  YAML_SEQUENCE_END_EVENT,
  YAML_MAPPING_START_EVENT,
  YAML_MAPPING_END_EVENT
};

struct ParserEvent
{
  EParserEventType type;
};

void Print(const ParserEvent& parserEvent)
{
}

bool Parse(Parser& parser, ParserEvent& parserEvent)
{
  parserEvent.type = EParserEventType::YAML_STREAM_END_EVENT;
  return true;
}

void BeginParse(char* str)
{
  Parser p = {};
  p.pAllocFn = malloc;
  p.pFreeFn = free;
  p.input = str;

  ParserEvent      event = {};
  EParserEventType eventType = EParserEventType::YAML_NO_EVENT;

  while (eventType != EParserEventType::YAML_STREAM_END_EVENT)
  {
    bool success = Parse(p, event);
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
  Parser parser;

  FILE* f = fopen("SampleScene.unity", "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char* string = (char*)malloc(fsize + 1);
  if (string)
  {
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = 0;

    BeginParse(string);
    free(string);
  }
}
