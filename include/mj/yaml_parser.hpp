#ifndef MJ_YAML_PARSER_H
#define MJ_YAML_PARSER_H

#include <stdint.h>

namespace mj
{
enum class EYamlEventType
{
  None,
  StreamStart,
  StreamEnd,
  DocumentStart,
  DocumentEnd,
  Alias,
  Scalar,
  SequenceStart,
  SequenceEnd,
  MappingStart,
  MappingEnd
};

enum class EYamlScalarStyle
{
  Any,
  Plain,
  SingleQuoted,
  DoubleQuoted,
  Literal,
  Folded
};

enum class EYamlSequenceStyle
{
  Any,
  Block,
  Flow
};

enum class EYamlMappingStyle
{
  Any,
  Block,
  Flow,
};

enum class EYamlEncoding
{
  Any,
  Utf8,
  Utf16Le,
  Utf16Be
};

struct YamlVersionDirective
{
  int32_t major;
  int32_t minor;
};

struct YamlTagDirective
{
  uint8_t* handle = nullptr;
  uint8_t* prefix = nullptr;
};

struct YamlMark
{
  size_t index  = 0;
  size_t line   = 0;
  size_t column = 0;
};

struct YamlEvent
{
  EYamlEventType type;

  union {
    struct
    {
      EYamlEncoding encoding;
    } stream_start;

    struct
    {
      YamlVersionDirective* version_directive;

      struct
      {
        YamlTagDirective* start;
        YamlTagDirective* end;
      } tag_directives;

      bool implicit;
    } document_start;

    struct
    {
      bool implicit;
    } document_end;

    struct
    {
      uint8_t* anchor;
    } alias;

    struct
    {
      uint8_t* anchor;
      uint8_t* tag;
      uint8_t* value;
      size_t length;
      bool plain_implicit;
      bool quoted_implicit;
      EYamlScalarStyle style;
    } scalar;

    struct
    {
      uint8_t* anchor;
      uint8_t* tag;
      int implicit;
      EYamlSequenceStyle style;
    } sequence_start;

    struct
    {
      uint8_t* anchor;
      uint8_t* tag;
      int implicit;
      EYamlMappingStyle style;
    } mapping_start;

  } data;

  YamlMark start_mark;
  YamlMark end_mark;

public:
  void Init(EYamlEventType type, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitStreamStart(EYamlEncoding encoding, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark);
  void InitDocumentStart(YamlVersionDirective version_directive, YamlTagDirective* tag_directives_start, YamlTagDirective* tag_directives_end, bool implicit, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitDocumentEnd(bool implicit, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitAlias(uint8_t* anchor, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitScalar(uint8_t* anchor, uint8_t* tag, uint8_t* value, size_t length, bool plain_implicit, bool quoted_implicit, EYamlScalarStyle style, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitSequenceStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlSequenceStyle style, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitSequenceEnd(const YamlMark& start_mark, const YamlMark& end_mark);
  void InitMappingStart(uint8_t* anchor, uint8_t* tag, bool implicit, EYamlMappingStyle style, const YamlMark& start_mark, const YamlMark& end_mark);
  void InitMappingEnd(const YamlMark& start_mark, const YamlMark& end_mark);
}; // YamlEvent

enum class EYamlTokenType
{
  None,
  StreamStart,
  StreamEnd,
  VersionDirective,
  TagDirective,
  DocumentStart,
  DocumentEnd,
  BlockSequenceStart,
  BlockMappingStart,
  BlockEnd,
  FlowSequenceStart,
  FlowSequenceEnd,
  FlowMappingStart,
  FlowMappingEnd,
  BlockEntry,
  FlowEntry,
  Key,
  Value,
  Alias,
  Anchor,
  Tag,
  Scalar
};

struct YamlToken
{
  EYamlTokenType type = EYamlTokenType::None;

  union {
    struct
    {
      EYamlEncoding encoding;
    } stream_start;

    struct
    {
      uint8_t* value;
    } alias;

    struct
    {
      uint8_t* value;
    } anchor;

    struct
    {
      uint8_t* handle;
      uint8_t* suffix;
    } tag;

    struct
    {
      uint8_t* value;
      size_t length;
      EYamlScalarStyle style;
    } scalar;

    struct
    {
      int major;
      int minor;
    } version_directive;

    YamlTagDirective tag_directive;

  } data;

  YamlMark start_mark;
  YamlMark end_mark;
};

enum class EYamlParserState
{
  StreamStart,
  ImplicitDocumentStart,
  DocumentStart,
  DocumentContent,
  DocumentEnd,
  BlockNode,
  BlockNodeOrIndentlessSequence,
  FlowNode,
  BlockSequenceFirstEntry,
  BlockSequenceEntry,
  IndentlessSequenceEntry,
  BlockMappingFirstKey,
  BlockMappingKey,
  BlockMappingValue,
  FlowSequenceFirstEntry,
  FlowSequenceEntry,
  FlowSequenceEntryMappingKey,
  FlowSequenceEntryMappingValue,
  FlowSequenceEntryMappingEnd,
  FlowMappingFirstKey,
  FlowMappingKey,
  FlowMappingValue,
  FlowMappingEmptyValue,
  End
};

struct YamlParser
{
  void* (*pAllocFn)(size_t) = nullptr;
  void (*pFreeFn)(void*)    = nullptr;
  char* input               = nullptr;
  EYamlParserState state    = EYamlParserState::StreamStart;

public:
  bool Parse(YamlEvent& parserEvent);

private:
  bool tokenAvailable = false;
  size_t tokensParsed = 0;

  bool FetchMoreTokens();
  void SkipToken();

  YamlToken* PeekToken();

  bool ParseStreamStart(YamlEvent& event);
  bool ParseDocumentStart(YamlEvent& event, bool isImplicit);
  bool ParseDocumentContent(YamlEvent& event);
  bool ParseDocumentEnd(YamlEvent& event);
  bool ParseNode(YamlEvent& event, bool isBlock, bool isIndentless);
  bool ParseBlockSequenceEntry(YamlEvent& event, bool isFirst);
  bool ParseIndentlessSequenceEntry(YamlEvent& event);
  bool ParseBlockMappingKey(YamlEvent& event, bool isFirst);
  bool ParseBlockMappingValue(YamlEvent& event);
  bool ParseFlowSequenceEntry(YamlEvent& event, bool isFirst);
  bool ParseFlowSequenceEntryMappingKey(YamlEvent& event);
  bool ParseFlowSequenceEntryMappingValue(YamlEvent& event);
  bool ParseFlowSequenceEntryMappingEnd(YamlEvent& event);
  bool ParseFlowMappingKey(YamlEvent& event, bool isFirst);
  bool ParseFlowMappingValue(YamlEvent& event, bool isEmpty);

  //yaml_error_type_t error;
  const char* problem;
  size_t problem_offset;
  int problem_value;
  YamlMark problem_mark;
  const char* context;
  YamlMark context_mark;

  bool stream_end_produced;

  struct
  {
    YamlToken* start;
    YamlToken* end;
    YamlToken* head;
    YamlToken* tail;
  } tokens;

  bool token_available;

  struct
  {
    EYamlParserState* start;
    EYamlParserState* end;
    EYamlParserState* top;
  } states;

  struct
  {
    YamlMark* start;
    YamlMark* end;
    YamlMark* top;
  } marks;

  struct
  {
    YamlTagDirective* start;
    YamlTagDirective* end;
    YamlTagDirective* top;
  } tag_directives;
};

} // namespace mj

#endif // MJ_YAML_PARSER_H
