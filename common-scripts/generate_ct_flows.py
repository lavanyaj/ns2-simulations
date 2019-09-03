import numpy as np
import argparse
parser = None
args = None


def parse_args():
    global parser
    global args
    parser = argparse.ArgumentParser(description="arguments to generate flows file")
    parser.add_argument("--seed1",type=int)#,default=8)
    parser.add_argument("--seed2",type=int)#,default=7)
    parser.add_argument("--num_servers",type=int)#,default=4)
    parser.add_argument("--flows_per_server",type=int)#,default=2)
    parser.add_argument("--frac_replace",type=float,default=0.4)
    parser.add_argument("--start_time",type=float)#,default=1.0)
    parser.add_argument("--epoch",type=float)#,default=0.002)
    parser.add_argument("--num_epochs",type=int)#,default=2)
    parser.add_argument("--num_bytes", type=int)#,default=100000000) # 8MB takes 8ms at 100G
    parser.add_argument("--flow_filename", type=str)#,default="ct-flows.tcl")
    args = parser.parse_args()
    pass

parse_args()
# we starts flows between random pairs of servers with 20 flows per server on average, after they have converged we replace 40 or 80% of the flows all at once.

prng1 = np.random.RandomState(args.seed1)
# used for picking flows to remove and start
prng2 = np.random.RandomState(args.seed2)
# used to pick random lengths for flows (when fixed length is false)
num_servers = args.num_servers
flows_per_server = args.flows_per_server
num_bytes = args.num_bytes

active_flows = {}
flow_gen = 0
start_time = args.start_time
curr_time = start_time
epoch = args.epoch # wait for 2ms (200 RTTs) before replacing

frac_replace = args.frac_replace
num_replace = int(round(frac_replace * flows_per_server * num_servers))
server_pairs = [(src, dst) for src in range(num_servers) for dst in range(num_servers) if not src == dst]
num_server_pairs = len(server_pairs)

with open(args.flow_filename, 'w') as file_:
    while curr_time < start_time + args.num_epochs * epoch:
        # pick a set of flows to replace
        if len(active_flows) > 0:
            active_flows_sorted = sorted(active_flows.keys())
            flows_to_stop = prng2.choice(active_flows_sorted, num_replace, replace=False)
            for f in flows_to_stop:
                src, dst = active_flows[f]
                file_.write("%d -1 %.6f %d %d\n"%\
                            (f, curr_time, src, dst))
                del active_flows[f]
                pass

        # generate new flows
        # initial set of flows
        if len(active_flows) == 0:
            num_new_flows = flows_per_server * num_servers
        else:
            num_new_flows = num_replace
            
            pass
        new_pair_indices= prng1.choice(range(num_server_pairs), num_new_flows, replace=True)
        new_pairs = [server_pairs[i] for i in new_pair_indices]

        for src,dst in new_pairs:
            flow_gen += 1
            active_flows[flow_gen] = (src, dst)
            file_.write("%d %d %.6f %d %d\n"%\
                    (flow_gen, num_bytes, curr_time, src, dst))
            pass

        curr_time += epoch
        pass
    pass
    
