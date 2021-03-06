#include "PF.h"

#ifdef MSSM_PROF
#include "profile.h"
#endif

std::vector<particle_cloud> PF
  (const problem_data &prob, const sampler &samp, const stats_comp_helper &trans)
{
#ifdef MSSM_PROF
  profiler prof("PF");
#endif

  std::vector<particle_cloud> out;
  out.reserve(prob.n_periods);
  const unsigned int trace = prob.ctrl.trace;

  for(arma::uword i = 0; i < prob.n_periods; ++i){
    if(i % 10L == 0)
      Rcpp::checkUserInterrupt();
    /* get conditional distribution at time i */
    std::unique_ptr<cdist> dist_t = prob.get_obs_dist(i);

    /* sample new cloud */
    if(i == 0)
      out.emplace_back(samp.sample_first(prob, *dist_t));
    else
      out.emplace_back(samp.sample      (prob, *dist_t, out.back(), i));

    particle_cloud &new_cloud = out.back();

    /* Update weights ad set stats */
    if(i > 0)
      trans.set_ll_n_stat(
        prob, *(out.rbegin() + 1), new_cloud, *dist_t, i);
    else
      trans.set_ll_n_stat(
        prob,                      new_cloud, *dist_t   );

    /* normalize weights */
    new_cloud.ws_normalized = new_cloud.ws;
    double ess = normalize_log_weights(new_cloud.ws_normalized);
    if(trace > 0){
      Rprintf("Effective sample size at %4d: %12.1f\n", i + 1L, ess);

      const arma::vec cloud_mean = new_cloud.get_cloud_mean();
      if(cloud_mean.n_elem < 20L or trace > 2)
        Rcpp::Rcout << "cloud mean: " << new_cloud.get_cloud_mean().t();
      arma::vec stats_mean = new_cloud.get_stats_mean();
      if(prob.ctrl.what_stat != log_densty and (
          stats_mean.n_elem < 20L or trace > 2)){
        const unsigned grad_dim =
          get_grad_dim(stats_mean.n_elem, prob.ctrl.what_stat);

        arma::vec grad(stats_mean.memptr(), grad_dim, false);
        Rcpp::Rcout << "Stats mean (gradient):\n" << grad.t();

        if(prob.ctrl.what_stat == Hessian){
          arma::mat hess
            (stats_mean.memptr() + grad_dim, grad_dim, grad_dim, false);
          Rcpp::Rcout << "Stats mean (Hessian):\n" << hess;

        }

      }

      Rcpp::Rcout << "log-likelihood contribution is: "
                  << arma::mean(new_cloud.ws) << '\n';

    }

    /* we do not need the olds stats anymore */
    if(i > 0L)
      (out.rbegin() + 1)->stats.clear();
  }

  return out;
}
