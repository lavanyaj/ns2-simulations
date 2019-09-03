import sys
import numpy 
import argparse
parser = None
args = None
eps=0.000001

def parse_args():
    global parser
    global args
    parser = argparse.ArgumentParser(description="arguments to generate flows file")
    parser.add_argument("--optimal_rates_file",type=str,default="optimal-rates.txt")
    parser.add_argument("--rate_changes_file",type=str,default="rates-changes.txt")
    parser.add_argument("--start_time",type=float,default=1.0)
    parser.add_argument("--epoch",type=float,default=0.002)
    parser.add_argument("--num_epochs",type=int,default=2)
    parser.add_argument("--dur",type=float,default=0.00002)
    parser.add_argument("--output_prefix",type=str,default="output")
    parser.add_argument("--get_rcp_rate", action="store_true")
    args = parser.parse_args()
    pass

parse_args()
optimal_rates_file = args.optimal_rates_file
actual_rates_file = args.rate_changes_file
prefix = args.output_prefix
start_time=args.start_time
epoch_start = start_time
epoch=args.epoch
num_epochs=args.num_epochs
end_time=start_time + epoch * num_epochs
dur = args.dur

# optimal_rates_file = "jup/wf-rate.txt"
# actual_rates_file = "jup/ack-rate-changes.txt" #"jup/ack2-sorted.txt"
# prefix = "output"
# start_time=1.0
# epoch_start = start_time
# epoch=0.002
# num_epochs=2
# end_time=start_time + epoch * num_epochs
# dur = 0.00002

last_line = None
last_line_act = None

def rel_err(val1, val2):
    if val1 < float('inf') and val2 < float('inf')\
       and val1 > 0 and val2 > 0:
        return (val1-val2)/val2
    return -1
    
def get_rates_header(opt_rates):
    active_flows = sorted(opt_rates.keys())
    
    ret = ",".join(["ActualRate-Flow%d, OptimalRate-Flow%d, RelError-Flow%d"%\
                    (f,f,f)\
                    for f in active_flows])
    
    return "time," + ret
    
def get_rates_str(time, actual_rates, opt_rates):
    active_flows = sorted(opt_rates.keys())
    
    for f in active_flows:
        if f not in actual_rates:
            actual_rates[f] = 0
        pass
        
    print "For time %.6f, active_flows %s\n"%(time, str(active_flows))
    
    ret = ",".join(["%.6f, %.6f, %.6f"%\
                    (actual_rates[f], opt_rates[f],\
                     rel_err(actual_rates[f], opt_rates[f]))\
                    for f in active_flows])
    return "%.6f,"%time + ret

def parse_line_actual_rate_change(line):
    if line.startswith("RECV_ACK"):
        return parse_line_act_recv_ack(line)
    else:        
        if (args.get_rcp_rate): 
            assert(line.startswith("RATE_CHANGE"))
            return parse_line_rcp_rate_change(line)
        else: 
            assert(line.startswith("#RATE_CHANGE"))
            return parse_line_sperc_rate_change(line)

# for RCP
# RECV_ACK fid time(s) ack_type is_exit rtt(us) num_dataPkts_received rate (Bytes/s)
# for s-PERC
# RECV_ACK fid time(s) ack_type is_exit rtt(us) num_dataPkts_received ??
def parse_line_act_recv_ack(line):
    words = line.rstrip().split()
    _, fid, time, _, _, _, _, rate = words
    fid = int(fid)
    time = float(time)
    rate = (float(rate)*8)/1.0e9
    return fid, time, rate


# #RATE_CHANGE fid time(s) sperc_data(mb/s) rcp_data(mb/s) rcp_ctrl(mb/s)  priority/ weight?
def parse_line_sperc_rate_change(line):
    words = line.rstrip().split()
    _, fid, time, rate, _, _, _ = words
    fid = int(fid)
    time = float(time)
    rate = (float(rate))/1.0e3
    return fid, time, rate
    
# RATE_CHANGE fid time(s) rcp_data(mb/s)
def parse_line_rcp_rate_change(line):
    words = line.rstrip().split()
    _, fid, time, rate = words
    fid = int(fid)
    time = float(time)
    rate = float(rate)/1.0e3
    return fid, time, rate

# RATE_CHANGE fid time(s) rcp_data(mb/s)
def parse_line_optimal_rate_change(line):
    words = line.rstrip().split()
    _, fid, time, rate = words
    fid = int(fid)
    time = float(time)
    rate = float(rate)#/1.0e3
    return fid, time, rate

# actual rates carry over from one epoch to another
# but we expect optimal rates to be updated every epoch
actual_rates = {}    
with open(actual_rates_file) as af, open(optimal_rates_file) as of:        
    while epoch_start + epoch <= end_time:
        print "get optimal rates for start %.6f\n"%\
            (epoch_start)
        optimal_rates = {}
        
        read_nextline = True        
        if last_line is not None:
            read_nextline = False
            fid, time, rate = parse_line_optimal_rate_change(last_line)
            if(time == epoch_start):
                read_nextline = True
                assert(fid not in optimal_rates)
                if rate > 0: optimal_rates[fid] = rate
                pass
            pass

        if read_nextline:
            for line in of:
                if "fid" in line: continue
                fid, time, rate = parse_line_optimal_rate_change(line)
                if (time > epoch_start + eps):
                    last_line = line
                    print "time %.6f > epoch_start + eps %.6f %s"%(time, (epoch_start+eps), line.rstrip())
                    break
                assert(fid not in optimal_rates)
                if rate > 0: optimal_rates[fid] = rate
                pass
            pass
            
        active_flows = optimal_rates.keys()
        print "%d active flows"%len(active_flows)
        # assume actual rate file sorted by time
        # intval maybe as long as 1 RTT
        # for each RTT, we'll log actual rates

        print "get actual rates for %.6f-%.6fs"\
            %(epoch_start, epoch_start+epoch)
        intval_start = epoch_start

        # what if ack isn't received, then we should
        # use last recorded rate, so maybe we want
        # to keep actual_rates as is.
        
        # log rates at intval_start+dur
        # line 1 intval_start+dur : ..
        # line 2 intval_start+2*dur: ..
        # last line epoch_start+epoch: ..
        print("writing rates file for %.6f-%.6fs\n"\
              % (epoch_start, epoch_start+epoch))


        with open("%s-%f.txt"%(prefix,epoch_start),\
                  "w") as ef:

            rates_header = get_rates_header(optimal_rates)
            ef.write(rates_header)
            ef.write("\n")
            
            while intval_start+dur <= epoch_start + epoch:
                #actual_rates = {}
                print "getting actual rates for %.6f-%.6fs\n"\
                    % (intval_start, intval_start+dur)

                read_nextline = True
                if last_line_act is not None:
                    read_nextline = False
                    fid, time, rate = parse_line_actual_rate_change(last_line_act)
                    if (time <= intval_start+dur+eps):
                        actual_rates[fid] = rate
                        read_nextline = True
                        pass
                    pass

                if read_nextline:                    
                    for line in af:
                        if "fid" in line: continue
                        fid, time, rate = parse_line_actual_rate_change(line)
                        print "fid %d, time %.6f, rate %.6f"%(fid, time, rate)
                        #if (time <= intval_start+eps):
                        #    print "time<=start %.6f: %s"%\
                        #        (intval_start, line)
                        #    break
                        #el
                        if (time > intval_start+dur+eps):
                            print "time> %.6f: %s"%\
                                (intval_start+dur, line)
                            last_line_act = line
                            break # out of for line in af, when time >> next intval_start
                        assert(time <= intval_start+dur+eps)

                        if (time <= intval_start):
                            print >> sys.stderr, "expected time to be in %.6f-%.6f: %s"%\
                                (intval_start,\
                                 intval_start+dur, line)
                        else:
                            print "time (%.6f-%.6f]: %s"%\
                                (intval_start,\
                                 intval_start+dur, line)
                            pass
                        actual_rates[fid] = rate
                        pass # for line in af
                    pass # if read_nextline
                print "writing rates for %.6f-%.6f:"% (intval_start, intval_start+dur)
                
                rates_str = get_rates_str(intval_start+dur, actual_rates, optimal_rates)                
                ef.write(rates_str)
                ef.write("\n")
                # write line for intval_start + dur

                intval_start += dur
                pass # intval_start+dur<=epoch_start+epoch
            pass # open(epoch_file)..

            epoch_start += epoch
        pass # epoch_start+epoch<=end_time
    pass # open(optimal_rates_file)..

                
    
