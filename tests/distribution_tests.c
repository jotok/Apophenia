/* These are tests of distributions. The basic idea is to 
 --assume a true set of parameters
 --generate a fake data set via a few thousand draws from your preferred model.
 --estimate the parameters of a new model using the fake data
 --assert that the estimated parameters are within epsilon of the true parameters.
*/
#include <apop.h>
#define Diff(L, R, eps) Apop_assert(fabs((L)-(R))<(eps), "%g is too different from %g (abitrary limit=%g).", (double)(L), (double)(R), eps);
int verbose = 1;
double nan_map(double in){return gsl_isnan(in);}

int estimate_model(apop_data *data, apop_model *dist, int method, apop_data *true_params){
    double *starting_pt;
    if(!strcmp(dist->name, "Bernoulli distribution"))
        starting_pt = (double[]){.5};
    else starting_pt = (double[]) {1.6, 1.4};

    Apop_model_add_group(dist, apop_mle, 
        .starting_pt = starting_pt,
        .method       = method, .verbose   =0,
        .step_size    = 1e-1,
        .tolerance    = 1e-4,   .k         = 1.8,
        .t_initial    = 1,      .t_min     = .5,
        .use_score    = 1
        );
    Apop_model_add_group(dist, apop_parts_wanted);
    if (verbose) printf( method==APOP_SIMPLEX_NM ? "Simplex [basic"
                        :method==APOP_CG_PR      ? "; Gradients [basic"
                        :method==APOP_RF_HYBRID  ? "; Newton's [basic"
                        : "default method [basic");
    fflush(NULL);

    if(!strcmp(dist->name, "Bernoulli distribution") && method==APOP_RF_HYBRID){
        printf(" skip]");
        return 0;
    }
    apop_model *e    = apop_estimate(data,*dist);
    Diff(0.0, apop_vector_distance(apop_data_pack(true_params),apop_data_pack(e->parameters)), 1e-1); 
    printf("][restart");fflush(NULL);
    e = apop_estimate_restart(e);
    Diff(0.0, apop_vector_distance(apop_data_pack(true_params),apop_data_pack(e->parameters)), 1e-1); 

        if (!strcmp(e->name, "Dirichlet distribution")
            || !strcmp(e->name, "Waring distribution")
            || !strcmp(e->name, "Gamma distribution") //just doesn't work.
            ||(!strcmp(e->name, "Bernoulli distribution") && method==APOP_RF_HYBRID)
            ||(!strcmp(e->name, "Exponential distribution")) //imprecise
            || !strcmp(e->name, "Yule distribution")){
            //cycle takes all day.
            printf("][skip EM]");fflush(NULL);
            return 0;
        }

    apop_model *dc  = apop_model_copy(*dist);
    Apop_settings_add(dc, apop_mle, tolerance, 1e-4);
    Apop_settings_add(dc, apop_mle, dim_cycle_tolerance, fabs(apop_log_likelihood(data, e))/200.); //w/in .5%.
    printf("][EM cycle");fflush(NULL);
    apop_model *dce = apop_estimate(data,*dc);
    printf("]");fflush(NULL);
    Diff(0.0, apop_vector_distance(apop_data_pack(true_params),apop_data_pack(dce->parameters)), 1e-2); 
    return 0;
}

/*Produce random data, then try to recover the original params */
void test_one_distribution(gsl_rng *r, apop_model *model, apop_model *true_params){
    long int runsize = 1e5;
    //generate.
    apop_data *data = apop_data_calloc(runsize,model->dsize);
    if (!strcmp(model->name, "Wishart distribution")){
        data = apop_data_calloc(runsize,4);
        true_params->parameters->vector->data[0] = runsize-4;
        for (size_t i=0; i< runsize; i++){
            Apop_row(data, i, v)
            true_params->draw(v->data, r, true_params);
            assert(!apop_vector_map_sum(v, nan_map));
        }
    } else if (!strcmp(model->name, "Binomial distribution") || !strcmp(model->name, "Multinomial distribution")){
        int n = gsl_rng_uniform(r)* 1e5;
        data = apop_data_calloc(runsize,true_params->parameters->vector->size);
        for (size_t i=0; i< runsize; i++){
            Apop_row(data, i, v)
            if (!strcmp(model->name, "Binomial distribution")){
                true_params->draw(gsl_vector_ptr(v, 1), r, true_params);
                v->data[0] = n - v->data[1];
            }
            true_params->draw(gsl_vector_ptr(v, 0), r, true_params);
            assert(!apop_vector_map_sum(v, nan_map));
        }
    } else {
        data = apop_data_calloc(runsize,model->dsize);
        for (size_t i=0; i< runsize; i++){
            Apop_row(data, i, v)
            true_params->draw(v->data, r, true_params);
            assert(!apop_vector_map_sum(v, nan_map));
        }
    }
    if (model->estimate) estimate_model(data, model,-3, true_params->parameters);
    else { //try all the MLEs.
        estimate_model(data, model,APOP_SIMPLEX_NM, true_params->parameters);
        estimate_model(data, model,APOP_CG_PR, true_params->parameters);
        estimate_model(data, model,APOP_RF_HYBRID, true_params->parameters);
    }
    apop_data_free(data);
}

void test_cdf(gsl_rng *r, apop_model *m){//m is parameterized
    //Make random draws from the dist, then find the CDF at that draw
    //That should generate a uniform distribution.
    if (!m->cdf || !strcmp(m->name, "Bernoulli distribution")
                || !strcmp(m->name, "Binomial distribution"))
        return;
    int drawct = 1e4;
    apop_data *draws = apop_data_alloc(drawct, m->dsize);
    apop_data *cdfs = apop_data_alloc(drawct);
    for (int i=0; i< drawct; i++){
        Apop_row(draws, i, onerow);
        apop_draw(onerow->data, r, m);
        Apop_data_row(draws, i, one_data_pt);
        apop_data_set(cdfs, i, -1, apop_cdf(one_data_pt, m));
    }
}

double true_parameter_v[] = {1.82,2.1};

void test_distributions(gsl_rng *r){
  if (verbose) printf("\n");
  apop_model* true_params;
  apop_model null_model = {"the null model"};
  apop_model *bernie_no_est = apop_model_copy(apop_bernoulli);
  bernie_no_est->estimate=NULL;
  apop_model *exp_no_est = apop_model_copy(apop_exponential);
  exp_no_est->estimate=NULL;
  apop_model *fish_no_est = apop_model_copy(apop_poisson);
  fish_no_est->estimate=NULL;
  apop_model dist[] = {
                apop_bernoulli, *bernie_no_est, apop_beta, 
                apop_binomial, /*apop_chi_squared,*/
                apop_dirichlet, apop_exponential, *exp_no_est,
                /*apop_f_distribution,*/
                apop_gamma, 
                apop_lognormal, apop_multinomial, apop_multivariate_normal,
                apop_normal, apop_poisson, *fish_no_est,
                /*apop_t_distribution,*/ apop_uniform,
                 /*apop_waring,*/ apop_yule, apop_zipf, 
                /*apop_wishart,*/
                null_model};

    for (int i=0; strcmp(dist[i].name, "the null model"); i++){
        if (verbose) {printf("%s: ", dist[i].name); fflush(NULL);}
        true_params   = apop_model_copy(dist[i]);
        true_params->parameters = apop_line_to_data(true_parameter_v, dist[i].vbase==1 ? 1 : 2,0,0);
        if (!strcmp(dist[i].name, "Dirichlet distribution"))
            dist[i].dsize=2;
        if (!strcmp(dist[i].name, "Beta distribution"))
            true_params->parameters = apop_line_to_data((double[]){.5, .2} , 2,0,0);
        if (!strcmp(dist[i].name, "Bernoulli distribution"))
            true_params->parameters = apop_line_to_data((double[]){.1} , 1,0,0);
        if (!strcmp(dist[i].name, "Binomial distribution")){
            true_params->parameters = apop_line_to_data((double[]){15, .2} , 2,0,0);
            dist[i].dsize=15;
        }
        if (!strcmp(dist[i].name, "Multivariate normal distribution")){
            true_params->parameters = apop_line_to_data((double[]){15, .5, .2,
                                                                    3, .2, .5} , 2,2,2);
            dist[i].dsize=2;
        }
        if (!strcmp(dist[i].name, "Multinomial distribution")){
            true_params->parameters = apop_line_to_data((double[]){15, .5, .2, .1} , 4,0,0);
            dist[i].dsize=15;
        }
        if (apop_regex(dist[i].name, "gamma distribution"))
            true_params->parameters = apop_line_to_data((double[]){1.5, 2.5} , 2,0,0);
        if (!strcmp(dist[i].name, "Chi squared distribution"))
            true_params->parameters = apop_line_to_data((double[]){996} , 1,0,0);
        if (!strcmp(dist[i].name, "F distribution"))
            true_params->parameters = apop_line_to_data((double[]){996, 996} , 2,0,0);
        if (!strcmp(dist[i].name, "t distribution"))
            true_params->parameters = apop_line_to_data((double[]){1, 3, 996} , 3,0,0);
        if (!strcmp(dist[i].name, "Wishart distribution")){
            true_params->parameters = apop_line_to_data((double[]){996, .2, .1,
                                                                     0, .1, .2}, 2,2,2);
            apop_vector_realloc(true_params->parameters->vector, 1);
        }
        test_one_distribution(r, dist+i, true_params);
        test_cdf(r, true_params);
        if (verbose) {printf("\nPASS.   "); fflush(NULL);}
    }
}
static void got_bored(){ exit(0); }

int main(int argc, char **argv){
    apop_opts.verbose=0;
    char c, opts[] = "sqt:";
    if (argc==1)
        printf("Distribution tests.\nFor quieter output, use -q. For multiple threads, use -t2, -t3, ...\n");
    while((c = getopt(argc, argv, opts))!=-1)
        if (c == 'q')      verbose  --;
        else if (c == 't') apop_opts.thread_count  = atoi(optarg);

    gsl_rng *r = apop_rng_alloc(213452);
    signal(SIGINT, got_bored);
    test_distributions(r);
}
