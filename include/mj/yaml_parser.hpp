#ifndef MJ_YAML_PARSER_H
#define MJ_YAML_PARSER_H

namespace mj
{
enum class EYamlParserEventType
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

struct YamlParserEvent
{
  EYamlParserEventType type;
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
  bool Parse(YamlParserEvent& parserEvent);

private:
  bool ParseStreamStart(YamlParserEvent& event);
  bool ParseDocumentStart(YamlParserEvent& event, bool isImplicit);
  bool ParseDocumentContent(YamlParserEvent& event);
  bool ParseDocumentEnd(YamlParserEvent& event);
  bool ParseNode(YamlParserEvent& event, bool isBlock, bool isIndentless);
  bool ParseBlockSequenceEntry(YamlParserEvent& event, bool isFirst);
  bool ParseIndentlessSequenceEntry(YamlParserEvent& event);
  bool ParseBlockMappingKey(YamlParserEvent& event, bool isFirst);
  bool ParseBlockMappingValue(YamlParserEvent& event);
  bool ParseFlowSequenceEntry(YamlParserEvent& event, bool isFirst);
  bool ParseFlowSequenceEntryMappingKey(YamlParserEvent& event);
  bool ParseFlowSequenceEntryMappingValue(YamlParserEvent& event);
  bool ParseFlowSequenceEntryMappingEnd(YamlParserEvent& event);
  bool ParseFlowMappingKey(YamlParserEvent& event, bool isFirst);
  bool ParseFlowMappingValue(YamlParserEvent& event, bool isEmpty);
};

} // namespace mj

#endif // MJ_YAML_PARSER_H
