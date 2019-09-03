import sys
import numpy 
import argparse

parser = argparse.ArgumentParser(description="arguments to generate flow rtts file")
parser.add_argument("--link_delays_filename",type=str)
parser.add_argument("--paths_filename",type=str)
parser.add_argument("--output_filename",type=str)
args = parser.parse_args()

link_delays = {}
with open(args.link_delays_filename) as link_delays_file:
    for line in link_delays_file.readlines():
        tokens = line.rstrip().split()
        link_delays["%s %s"%(tokens[0], tokens[1])] = float(tokens[2])
        #break
    pass

path_rtts = {}
with open(args.paths_filename, 'r') as paths_file, open(args.output_filename, 'w') as output_file:
    for line in paths_file.readlines():
        print line

        tokens = line.rstrip().split()
        flow = tokens[0]

        path = " ".join(tokens[3:])

        print path

        fwd_links =  (["%s %s"%(l,m) for l,m in zip(tokens[3:], tokens[4:])])
        rvs_links = (["%s %s"%(m,l) for l,m in zip(tokens[3:], tokens[4:])])
        rvs_links =  [l for l in reversed(rvs_links)]
        
        print fwd_links
        print rvs_links

        fwd_delay = sum ([link_delays[link] for link in fwd_links])
        rvs_delay = sum ([link_delays[link] for link in rvs_links])
        print fwd_delay+rvs_delay

        output_file.write("%s %s %s\n"%(flow, (fwd_delay+rvs_delay), len(fwd_links)))

    pass
