#include "mj/yaml.hpp"

#include <stdio.h>
#include <stdlib.h>

#define INDENT "  "
#define STRVAL(x) ((x) ? (char*)(x) : "")

void indent(int level)
{
  int i;
  for (i = 0; i < level; i++)
  {
    printf("%s", INDENT);
  }
}

void print_event(const mj::YamlEvent& event)
{
  static int level = 0;

  switch (event.type)
  {
  case mj::EYamlEventType::None:
    indent(level);
    printf("no-event\n");
    break;
  case mj::EYamlEventType::StreamStart:
    indent(level++);
    printf("stream-start-event\n");
    break;
  case mj::EYamlEventType::StreamEnd:
    indent(--level);
    printf("stream-end-event\n");
    break;
  case mj::EYamlEventType::DocumentStart:
    indent(level++);
    printf("document-start-event\n");
    break;
  case mj::EYamlEventType::DocumentEnd:
    indent(--level);
    printf("document-end-event\n");
    break;
  case mj::EYamlEventType::Alias:
    indent(level);
    printf("alias-event\n");
    break;
  case mj::EYamlEventType::Scalar:
    indent(level);
    printf("scalar-event={value=\"%s\", length=%d}\n",
      STRVAL(std::get<mj::YamlEvent::scalar_t>(event.data).value),
      (int)std::get<mj::YamlEvent::scalar_t>(event.data).length);
    break;
  case mj::EYamlEventType::SequenceStart:
    indent(level++);
    printf("sequence-start-event\n");
    break;
  case mj::EYamlEventType::SequenceEnd:
    indent(--level);
    printf("sequence-end-event\n");
    break;
  case mj::EYamlEventType::MappingStart:
    indent(level++);
    printf("mapping-start-event\n");
    break;
  case mj::EYamlEventType::MappingEnd:
    indent(--level);
    printf("mapping-end-event\n");
    break;
  }
  if (level < 0)
  {
    fprintf(stderr, "indentation underflow!\n");
    level = 0;
  }
}

void BeginParse(char* str, size_t size)
{
  mj::YamlFns Fns;
  Fns.Malloc = malloc;
  Fns.Realloc = realloc;
  Fns.Free = free;
  Fns.Strdup = _strdup;
  mj::YamlParser p(Fns, (const unsigned char*)str, size);

  //p.input    = str;

  mj::YamlEvent event          = {};
  mj::EYamlEventType eventType = mj::EYamlEventType::None;

  while (eventType != mj::EYamlEventType::StreamEnd)
  {
    bool success = p.Parse(event);
    if (success)
    {
      print_event(event);
      eventType = event.type;
      event.Delete(p);
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

    BeginParse(string, fsize);
    free(string);
  }
}
