# ns2-simulations
NS-2 simulations comparing proactive and reactive algorithms, for SIGMETRICS'19 paper

## Installation
I used Ubuntu 14.04 LTS on to run these simulations on multi-CPU servers.

Run ./setup.sh to install NS-2 binaries for s-PERC, RCP and DCTCP.

Install jupyter notebook to plot the results (you do not need anaconda).

## Running experiments

Change to common-scripts directory. 

Then use run_experiment.sh to run one of the two kinds of NS-2 experiments from the paper:
- ct : convergence times (CT) in Datacenter Fabrics and Wide-Area Networks
- fct: flow completion times (FCT) in Datacenter Fabrics

For the FCT experiments, you can adjust which schemes, workload and load you want to run in the setup_for_fct function in run_experiment.sh.

For the CT experiments, you can adjust which scheme, network and churn you want to try by including the relevant config in the CONFIGS array
in the setup_for_ct function in run_experiment.sh. 

The script will run experiments in parallel. 
You can set max_cpus in the setup_for_ct/ setup_for_fct functions to control how many experiments to run in parallel.
I ran each experiment on its own server with MAX_CPUS set to 20. It can take one or two days to finish all the runs.
You can adjust NUM_RUNS_EACH to run a subset of the runs.

## Running new experiments

The scripts by default use the same settings (for each experiment and each scheme) as in the paper. You can change these settings
to run new versions of the schemes in new scenarios. The scheme specific settings are in the respective directories for the scheme.
To run s-PERC with different settings, add a new version of s-PERC in the versions array in 
sperc-scripts/files_to_run/drive_sperc_ct.sh (or sperc-scripts/files_to_run/run_sperc_fct.sh). Use the KW (keyword) field to
specify the name of this setting. Then, to run convergence time experiments with the new version, add a new config with the same KW
to the CONFIGS array in the setup_for_ct function in common_scripts/run_experiment.sh

## Plotting results
Use common_scripts/compare_fcts.ipynb to plot results from the FCT experiment.
