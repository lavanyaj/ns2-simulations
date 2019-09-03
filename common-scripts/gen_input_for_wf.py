import numpy as np
import argparse
parser = None
args = None

def parse_args():
    global parser
    global args
    parser = argparse.ArgumentParser(description="arguments to generate flows file")
    parser.add_argument("--flow_filename", type=str,default="ct-flows.tcl")
    parser.add_argument("--paths_filename", type=str,default="paths.tcl")
    parser.add_argument("--wf_input_filename", type=str,default="ct-flows-paths.tcl")

    args = parser.parse_args()
    pass

parse_args()
flow_filename = args.flow_filename
paths_filename = args.paths_filename
wf_input_filename = args.wf_input_filename
# open(flow_filename, 'r') as ff,
paths = {}
# paths[1] = 0 144 2 (fwd path including src and dst)
with open(paths_filename, 'r') as pf:
    for line in pf:
        if line.startswith("path of flow"):        
            fwd = line.split("/")[0]
            tokens = fwd.split(" ")
            flow_id = int(tokens[3])
            assert(len(tokens) > 6)
            paths[flow_id] = [int(f) for f in tokens[5:-2]]
            print ("parse fwd %s to get path of flow %d [3] is [5:-2] %s"%(fwd, flow_id, str(paths[flow_id])))
            pass
        elif line.startswith("SPERC_CTRL_SYN_REV"):
            fwd = line.split("/")[0]
            tokens = fwd.split(" ")            
            flow_id = int(tokens[2+4])            
            assert(len(tokens) > 14+4)
            assert(tokens[12+4] == "path")
            paths[flow_id] = [int(f) for f in tokens[13+4:-2]]
            print ("parse fwd %s to get path of flow %d [6] is [17:-2] %s"%(fwd, flow_id, str(paths[flow_id])))
        pass
    pass

with open(flow_filename, 'r') as ff, open(wf_input_filename, 'w') as wif:
    for line in ff:
        tokens = line.rstrip().split()
        flow_id = int(tokens[0])
        num_bytes = int(tokens[1])
        time = tokens[2]
        src = int(tokens[3])
        dst = int(tokens[-1])
        if (flow_id not in paths):
            print ("flow %d not in paths, line from flow_filename %s\n"%\
                   (flow_id, line.rstrip()))
        assert(flow_id in paths)
        if not (paths[flow_id][0] == src):
            print ("path of flow %d doesn't start with src %d, line from flow_filename %s\n"%\
                   (flow_id, src, line.rstrip()))
        assert(paths[flow_id][0] == src)

        if not (paths[flow_id][-1] == dst):
            print ("path of flow %d doesn't end with dst %d, line from flow_filename %s\n"%\
                   (flow_id, dst, line.rstrip()))
        assert(paths[flow_id][-1] == dst)
        new_tokens = [flow_id, num_bytes, time]
        new_tokens.extend(paths[flow_id])
        
        new_line = " ".join([str(t) for t in new_tokens])
        #print new_line
        wif.write(new_line)
        wif.write("\n")
        pass
    pass
    
