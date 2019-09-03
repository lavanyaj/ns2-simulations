import numpy as np
import matplotlib
from matplotlib import pyplot as plt
#import mpld3
#from mpld3 import plugins
import pandas as pd
import argparse
import sys
#from utils import *
OUT_DIR="."
"""
how to use to get CT results
python files_to_run/get_convergence_times.py --filename results/*flow-rate*txt
"""
parser = argparse.ArgumentParser()
parser.add_argument("--filename", type=str, nargs='+', help="CSV file of link rates")
parser.add_argument("--stdin", action="store_true")
parser.add_argument("--thresh", type=float, default=0.1)
parser.add_argument("--frac_flows", type=float, default=0.01)
parser.add_argument("--output_filename", type=str)

args = parser.parse_args()
thresholds = [args.thresh] #[x * 0.1 for x in range(10)] 

rel_err_prefix=" RelErr"
time_header="time"
time_unit="s"

def get_filename(prefix):
    return "%s/get_convergence_times-log-%s.txt"%\
        (OUT_DIR, prefix)
    pass

def get_headers():
    return "filename," + ",".join(["convergence_time-%d, converged_for-%s, last_to_converge-%s"\
                     % (t*100, t*100, t*100) for t in thresholds])+"\n"

def get_convergence_times(filename,stdin):
    run_id = filename.split("/")[-1]
    maxCpg1Level = -1
    maxCpg2Level = -1
    maxWfLevel = -1
    maxRtt = -1
    distinctOptimalRates = -1
    with open(filename) as f:
        for line in f:
            if line.startswith("# maxCpg1Level"):
                maxCpg1Level = int(line.rstrip().split()[-1])
                pass
            if line.startswith("# maxCpg2Level"):
                maxCpg2Level = int(line.rstrip().split()[-1])
                pass
            if line.startswith("# maxWfLevel"):
                maxWfLevel = int(line.rstrip().split()[-1])
                pass

            if line.startswith("# maxRtt"):
                maxRtt = float(line.rstrip().split()[-1])
                pass                
            if line.startswith("# distinctOptimalRates"):
                distinctOptimalRates = int(line.rstrip().split()[-1])
                pass                

            pass
        pass

    if stdin:
        df = pd.read_csv(sys.stdin, header='infer', comment='#')
    else:
        df = pd.read_csv(filename, header='infer', comment='#')
        pass

    #print df.head
    
    num_columns = df.shape[1]
    print df.dtypes
    epoch_start_time = df.time.iloc[[0]].values[0]
    start_time = 0    
    last_time = df.time.iloc[[-1]].values[0]
    print "epoch start", epoch_start_time
    print "last time", last_time

    relative_errs= df.select(lambda col: col.startswith(rel_err_prefix),axis=1)
    abs_relative_errs = relative_errs.apply(abs, axis=1)
    max_relative_errs = abs_relative_errs.apply(max, axis=1)
    max_relative_errs_index = abs_relative_errs.idxmax(axis=1)
    
    num_rows = max_relative_errs.shape[0]
    for row in range(124,150):
        print "row", row,\
            "time", df.time.iloc[[row]].values[0],\
            "max rel err",\
            max(abs_relative_errs.iloc[row,:].values),\
            "argmax", max_relative_errs_index[row],\
            "actual val",\
            abs_relative_errs.iloc[row,:][max_relative_errs_index[row]]
        pass

    convergence_durations = []
    convergence_times = []
    convergence_row = []
    convergence_times_from_epoch_start = []
    
    convergence_time_index = []
    last_time_over = []
    last_time_over_index = []
    num_flows_over_index = []
    for t in thresholds:
        last_time_over.append(-1)
        convergence_time_index.append(-1)
        last_time_over_index.append(-1)
        num_flows_over_index.append(-1)
        
    #last_time = max_relative_errs.index[num_rows-1]

    # for each threshold: we store last row when the max relative
    # error (out of all flows' relative errors) exceeded the threshold
    # and we also store the argmax (or "index"). convergence_time
    # stores tentative row when rel errors converged to within threshold
    # (tentative because we might encounter a row later, when rel err
    # is more than threshold)
    # if last_time_over is -1, that means max relative error was 
    # always <= threshold

    # for each threshold: we store last row when at least 1% of flows' relative errors exceeded the threshold

    num_flows = abs_relative_errs.shape[1] # time x flows
    thresh_flows = int(args.frac_flows * num_flows)

    print "total", num_flows, "flows, threshold is", thresh_flows, " args.frac_flows is ", args.frac_flows
    #print "shape of abs_rel", abs_relative_errs.shape, type(abs_relative_errs)
    if (args.frac_flows > 0 and num_flows >= 20): assert(thresh_flows > 0)

    for i in range(num_rows):
        val = max_relative_errs[i]
        for t in range(len(thresholds)):
            flows_exceeding = np.where(abs_relative_errs.iloc[i,:].values > thresholds[t])[0]
            print "row ", i, "has", len(flows_exceeding), "flows with abs relative error exceeding", thresholds[t]

            if i == 0 and len(flows_exceeding) <= thresh_flows: 
                print "setting convergence time indexe for", thresholds[t], " to 0 because row 0 satisfies"
                convergence_time_index[t] = 0
            if len(flows_exceeding) > thresh_flows:
            #if val > thresholds[t]:
                last_time_over[t] = i
                convergence_time_index[t] = i+1
                last_time_over_index[t] = max_relative_errs_index[i]
                num_flows_over_index[t] = len(flows_exceeding)
            print "row %d, threshold %f, max_relative_err %f, flow %s, num flows %d, time %s"\
                %(i, thresholds[t], val,str(max_relative_errs_index[i]),len(flows_exceeding), str(df.time.iloc[[i]].values[0]))

    for t in range(len(thresholds)):
        ct_index = convergence_time_index[t] # last row wgen 1% flows were off
        print "ct_index for threshold",t,"is",ct_index
        
        ct = -1
        cd = -1
        ct_from_epoch_start = -1
        # convergence time: last time before the experiment end when
        # a flow's relative error exceeded the threshold (-1 implies
        # all flows had relative errors within threshold for duration of
        # experiment). convergence: duration: how long since a
        # flow's relative error exceeded the threshold
        if ct_index >= 0 and ct_index < num_rows:
            convergence_time = df.time.iloc[[ct_index]].values[0] #max_relative_errs.index[ct_index]
            print "cnvergence time is",convergence_time
            ct = ((convergence_time-start_time)) #.microseconds
            cd = ((last_time-convergence_time)) #.microseconds
            ct_from_epoch_start = ((convergence_time-epoch_start_time)) #.microseconds
            if len(convergence_times) > 0: rel_ct = ct - convergence_times[-1]
            if maxRtt > 0:
                ct = ct
                cd = cd
                ct_from_epoch_start = ct_from_epoch_start
            
        convergence_times.append(ct)
        convergence_durations.append(cd)
        convergence_row.append(ct_index)
        convergence_times_from_epoch_start.append(ct_from_epoch_start)

    if min(convergence_times_from_epoch_start) < 0.1:
        # for DC scenario where RTT is 10us 
        format_str = "%.6f, %.6f, %.6f, %s, %.6f, %s"
    else:
        # for WAN like scenarios where RTT is 100ms
        format_str = "%.2f, %.2f, %.2f, %s, %.2f, %s"

    return run_id + ", " + ",".join([format_str\
                                     % (((convergence_times_from_epoch_start[t])),\
                                        ((convergence_times[t])),\
                                        ((convergence_durations[t])),\
                                        str(last_time_over_index[t]),\
                                        (num_flows_over_index[t]),\
                                        str(convergence_row[t]))\
                                     for t in range(len(thresholds))])

    # return run_id + ", " + str(maxCpg1Level) +", " + str(maxCpg2Level) + ", " + str(maxWfLevel)\
    #     + ", " + "%.1f"%maxRtt\
    #     + ", " + "%d"%distinctOptimalRates\
    #     + ", " + ",".join(["%s, %s, %s, %s, %s, %s"\
    #                        % (str((convergence_times_from_epoch_start[t])),\
    #                           str((convergence_times[t])),\
    #                           str((convergence_durations[t])),\
    #                           str(last_time_over_index[t]),\
    #                           str(num_flows_over_index[t]),\
    #                           str(convergence_row[t]))\
    #                        for t in range(len(thresholds))])
   

if (args.stdin):
    assert(len(args.filename) == 1)

with open(args.output_filename, "w") as f:
    for filename in args.filename:
        str_ = "%s"%(get_convergence_times(filename, stdin=args.stdin))
        #f.write(filename)
        f.write("thresh=%d"%(args.thresh*100))
        f.write(", frac=%d, "%(100-(args.frac_flows*100)))
        f.write(str_)        
        f.write("\n")
        pass
    pass
    
