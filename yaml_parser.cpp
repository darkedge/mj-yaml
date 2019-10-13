#include "mj/yaml_parser.hpp"

using namespace mj;

bool YamlParser::Parse(ParserEvent& parserEvent)
{
  switch (this->state)
  {
  case EParserState::StreamStart:
    break;
  case EParserState::ImplicitDocumentStart:
    break;
  case EParserState::DocumentStart:
    break;
  case EParserState::DocumentContent:
    break;
  case EParserState::DocumentEnd:
    break;
  case EParserState::BlockNode:
    break;
  case EParserState::BlockNodeOrIndentlessSequence:
    break;
  case EParserState::FlowNode:
    break;
  case EParserState::BlockSequenceFirstEntry:
    break;
  case EParserState::BlockSequenceEntry:
    break;
  case EParserState::IndentlessSequenceEntry:
    break;
  case EParserState::BlockMappingFirstKey:
    break;
  case EParserState::BlockMappingKey:
    break;
  case EParserState::BlockMappingValue:
    break;
  case EParserState::FlowSequenceFirstEntry:
    break;
  case EParserState::FlowSequenceEntry:
    break;
  case EParserState::FlowSequenceEntryMappingKey:
    break;
  case EParserState::FlowSequenceEntryMappingValue:
    break;
  case EParserState::FlowSequenceEntryMappingEnd:
    break;
  case EParserState::FlowMappingFirstKey:
    break;
  case EParserState::FlowMappingKey:
    break;
  case EParserState::FlowMappingValue:
    break;
  case EParserState::FlowMappingEmptyValue:
    break;
  case EParserState::End:
    break;
  default:
    break;
  }
  parserEvent.type = EParserEventType::StreamEnd;
  return true;
}