#ifndef MJ_YAML_PARSER_H
#define MJ_YAML_PARSER_H

namespace mj
{
enum class EParserEventType
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

struct ParserEvent
{
  EParserEventType type;
};

enum class EParserState
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
  EParserState state        = EParserState::StreamStart;

  bool Parse(ParserEvent& parserEvent);
};

} // namespace mj

#endif // MJ_YAML_PARSER_H
