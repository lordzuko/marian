#include "cpu/decoder/encoder_decoder.h"
#include "cpu/decoder/encoder_decoder_loader.h"

#include <vector>
#include <yaml-cpp/yaml.h>

#include "common/threadpool.h"

#include "cpu/dl4mt/dl4mt.h"

#include "common/god.h"
#include "common/loader.h"
#include "common/scorer.h"
#include "common/sentence.h"
#include "common/sentences.h"

#include "cpu/mblas/matrix.h"
#include "cpu/decoder/best_hyps.h"

using namespace std;

namespace amunmt {

namespace CPU {

using EDState = EncoderDecoderState;

////////////////////////////////////////////////
EncoderDecoderState::EncoderDecoderState()
{
}

std::string EncoderDecoderState::Debug() const
{
  return CPU::mblas::Debug(states_);
}

CPU::mblas::Matrix& EncoderDecoderState::GetStates() {
  return states_;
}

CPU::mblas::Matrix& EncoderDecoderState::GetEmbeddings() {
  return embeddings_;
}

const CPU::mblas::Matrix& EncoderDecoderState::GetStates() const {
  return states_;
}

const CPU::mblas::Matrix& EncoderDecoderState::GetEmbeddings() const {
  return embeddings_;
}

void EncoderDecoderState::JoinStates(const States& states) {
    std::cerr << "EncoderDecoderState::JoinStates: NOT Implemented " << std::endl;
}


////////////////////////////////////////////////
EncoderDecoder::EncoderDecoder(const std::string& name,
                               const YAML::Node& config,
                               const DeviceInfo& deviceInfo,
                               size_t tab,
                               const Weights& model)
  : Scorer(name, config, deviceInfo,tab),
    model_(model),
    encoder_(new CPU::Encoder(model_)),
    decoder_(new CPU::Decoder(model_))
{}

void EncoderDecoder::Decode(const God &god, const State& in, State& out) {
  const EDState& edIn = in.get<EDState>();
  EDState& edOut = out.get<EDState>();

  decoder_->Decode(edOut.GetStates(), edIn.GetStates(),
                     edIn.GetEmbeddings(), SourceContext_);
}

void EncoderDecoder::Decode(const God &god, const State& in, State& out, const std::vector<size_t>&) {
  Decode(god, in, out);
}

State* EncoderDecoder::NewState() const {
  return new EDState();
}

void EncoderDecoder::BeginSentenceState(State& state, size_t batchSize) {
  EDState& edState = state.get<EDState>();
  decoder_->EmptyState(edState.GetStates(), SourceContext_, batchSize);
  decoder_->EmptyEmbedding(edState.GetEmbeddings(), batchSize);
}

void EncoderDecoder::SetSource(const Sentences& sources) {
  encoder_->GetContext(sources.at(0)->GetWords(tab_),
                        SourceContext_);
}

void EncoderDecoder::AssembleBeamState(const State& in,
                                       const Beam& beam,
                                       State& out) {
  std::vector<size_t> beamWords;
  std::vector<size_t> beamStateIds;
  for(auto h : beam) {
      beamWords.push_back(h->GetWord());
      beamStateIds.push_back(h->GetPrevStateIndex());
  }

  const EDState& edIn = in.get<EDState>();
  EDState& edOut = out.get<EDState>();

  edOut.GetStates() = mblas::Assemble<mblas::byRow, mblas::Matrix>(edIn.GetStates(), beamStateIds);
  decoder_->Lookup(edOut.GetEmbeddings(), beamWords);
}

void EncoderDecoder::GetAttention(mblas::Matrix& Attention) {
  decoder_->GetAttention(Attention);
}

mblas::Matrix& EncoderDecoder::GetAttention() {
  return decoder_->GetAttention();
}

size_t EncoderDecoder::GetVocabSize() const {
  return decoder_->GetVocabSize();
}

void EncoderDecoder::Filter(const std::vector<size_t>& filterIds) {
  decoder_->Filter(filterIds);
}

Encoder& EncoderDecoder::GetEncoder() {
  return *encoder_;
}

Decoder& EncoderDecoder::GetDecoder() {
  return *decoder_;
}

BaseMatrix& EncoderDecoder::GetProbs() {
  return decoder_->GetProbs();
}

std::vector<float> EncoderDecoder::GetScores(const std::vector<std::pair<size_t, size_t>>& ids) {
    std::cerr << "EncoderDecoder::GetScores: NOT IMPLEMENTED" << std::endl;
    return std::vector<float>(ids.size(), 0.0f);
}


////////////////////////////////////////////////
EncoderDecoderLoader::EncoderDecoderLoader(const std::string name,
                                           const YAML::Node& config)
  : Loader(name, config) {}

void EncoderDecoderLoader::Load(const God&) {
  std::string path = Get<std::string>("path");

  LOG(info, "Loading model {}", path);
  weights_.emplace_back(new Weights(path, 0));
}

ScorerPtr EncoderDecoderLoader::NewScorer(const God&, const DeviceInfo &deviceInfo) const {
  size_t tab = Has("tab") ? Get<size_t>("tab") : 0;
  return ScorerPtr(new EncoderDecoder(name_, config_, deviceInfo,
                                      tab, *weights_[0]));
}

BestHypsBasePtr EncoderDecoderLoader::GetBestHyps(const God&) const {
  return BestHypsBasePtr(new CPU::BestHyps());
}

}

}

