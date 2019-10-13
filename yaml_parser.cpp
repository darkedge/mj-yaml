#include "mj/yaml_parser.hpp"

using namespace mj;

bool YamlParser::ParseStreamStart(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseDocumentStart(YamlParserEvent& event, bool isImplicit)
{
  return true;
}

bool YamlParser::ParseDocumentContent(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseDocumentEnd(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseNode(YamlParserEvent& event, bool isBlock, bool isIndentless)
{
  return true;
}

bool YamlParser::ParseBlockSequenceEntry(YamlParserEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseIndentlessSequenceEntry(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseBlockMappingKey(YamlParserEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseBlockMappingValue(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntry(YamlParserEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingKey(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingValue(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingEnd(YamlParserEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowMappingKey(YamlParserEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowMappingValue(YamlParserEvent& event, bool isEmpty)
{
  return true;
}

bool YamlParser::Parse(YamlParserEvent& event)
{
  switch (this->state)
  {
  case EYamlParserState::StreamStart:
    return this->ParseStreamStart(event);
  case EYamlParserState::ImplicitDocumentStart:
    return this->ParseDocumentStart(event, true);
  case EYamlParserState::DocumentStart:
    return this->ParseDocumentStart(event, false);
  case EYamlParserState::DocumentContent:
    return this->ParseDocumentContent(event);
  case EYamlParserState::DocumentEnd:
    return this->ParseDocumentEnd(event);
  case EYamlParserState::BlockNode:
    return this->ParseNode(event, true, false);
  case EYamlParserState::BlockNodeOrIndentlessSequence:
    return this->ParseNode(event, true, true);
  case EYamlParserState::FlowNode:
    return this->ParseNode(event, false, false);
  case EYamlParserState::BlockSequenceFirstEntry:
    return this->ParseBlockSequenceEntry(event, true);
  case EYamlParserState::BlockSequenceEntry:
    return this->ParseBlockSequenceEntry(event, false);
  case EYamlParserState::IndentlessSequenceEntry:
    return this->ParseIndentlessSequenceEntry(event);
  case EYamlParserState::BlockMappingFirstKey:
    return this->ParseBlockMappingKey(event, true);
  case EYamlParserState::BlockMappingKey:
    return this->ParseBlockMappingKey(event, false);
  case EYamlParserState::BlockMappingValue:
    return this->ParseBlockMappingValue(event);
  case EYamlParserState::FlowSequenceFirstEntry:
    return this->ParseFlowSequenceEntry(event, true);
  case EYamlParserState::FlowSequenceEntry:
    return this->ParseFlowSequenceEntry(event, false);
  case EYamlParserState::FlowSequenceEntryMappingKey:
    return this->ParseFlowSequenceEntryMappingKey(event);
  case EYamlParserState::FlowSequenceEntryMappingValue:
    return this->ParseFlowSequenceEntryMappingValue(event);
  case EYamlParserState::FlowSequenceEntryMappingEnd:
    return this->ParseFlowSequenceEntryMappingEnd(event);
  case EYamlParserState::FlowMappingFirstKey:
    return this->ParseFlowMappingKey(event, true);
  case EYamlParserState::FlowMappingKey:
    return this->ParseFlowMappingKey(event, false);
  case EYamlParserState::FlowMappingValue:
    return this->ParseFlowMappingValue(event, false);
  case EYamlParserState::FlowMappingEmptyValue:
    return this->ParseFlowMappingValue(event, true);
  case EYamlParserState::End:
    break;
  default:
    break;
  }

  event.type = EYamlParserEventType::StreamEnd;
  return true;
}