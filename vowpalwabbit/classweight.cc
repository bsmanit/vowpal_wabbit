#include <unordered_map>
#include "reductions.h"
using namespace std;

namespace CLASSWEIGHTS
{
struct classweights
{
  std::unordered_map<uint32_t, float> weights;

  void load_string(std::string const& source)
  {
    std::stringstream ss(source);
    std::string item;
    while (std::getline(ss, item, ','))
    {
      std::stringstream inner_ss(item);
      std::string klass;
      std::string weight;
      std::getline(inner_ss, klass, ':');
      std::getline(inner_ss, weight, ':');

      if (!klass.size() || !weight.size())
      {
        THROW("error: while parsing --classweight " << item);
      }

      int klass_int = std::stoi(klass);
      float weight_double = std::stof(weight);

      weights[klass_int] = weight_double;
    }
  }

  float get_class_weight(uint32_t klass)
  {
    auto got = weights.find(klass);
    if ( got == weights.end() )
      return 1.0f;
    else
      return got->second;
  }
};

template <bool is_learn, int pred_type>
static void predict_or_learn(classweights& cweights, LEARNER::base_learner& base, example& ec)
{
  switch (pred_type)
  {
  case prediction_type::scalar:
    ec.weight *= cweights.get_class_weight((uint32_t)ec.l.simple.label);
    break;
  case prediction_type::multiclass:
    ec.weight *= cweights.get_class_weight(ec.l.multi.label);
    break;
  default:
    // suppress the warning
    break;
  }

  if (is_learn)
    base.learn(ec);
  else
    base.predict(ec);
}

void finish(classweights& data) { data.weights.~unordered_map();}
}

using namespace CLASSWEIGHTS;

LEARNER::base_learner* classweight_setup(vw& all)
{
  if (missing_option<vector<string> >(all, "classweight", "importance weight multiplier for class"))
    return nullptr;

  vector<string> classweight_array = all.vm["classweight"].as< vector<string> >();

  classweights& cweights = calloc_or_throw<classweights>();
  new (&(cweights.weights)) std::unordered_map<uint32_t,float>();

  for (auto& s : classweight_array) {
    *all.file_options << " --classweight " << s;
    cweights.load_string(s);
  }

  if (!all.quiet)
    all.trace_message << "parsed " << cweights.weights.size() << " class weights" << endl;

  LEARNER::base_learner* base = setup_base(all);

  LEARNER::learner<classweights>* ret;

  if (base->pred_type == prediction_type::scalar)
    ret = &LEARNER::init_learner<classweights>(&cweights, base, predict_or_learn<true,prediction_type::scalar>, predict_or_learn<false,prediction_type::scalar>);
  else if (base->pred_type == prediction_type::multiclass)
    ret = &LEARNER::init_learner<classweights>(&cweights, base, predict_or_learn<true,prediction_type::multiclass>, predict_or_learn<false,prediction_type::multiclass>);
  else
    THROW("--classweight not implemented for this type of prediction");
  ret->set_finish(finish);
  return make_base(*ret);
}
