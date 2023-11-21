from abc_py_ import AbcInterface_
import numpy as np
import dgl
import torch

class AbcInterface(AbcInterface_):
    '''
    ver = t.i
    t: node type encoding
      0: combinational only
      1: supports sequential
      2: uses CI/CO instead of PI/PO/BI/BO
    i: inverter encoding
      0: as node type
      1: as node feature
      2: as edge feature
    '''
    def dgl_graph(self, ver):
        tver, iver = int(ver), int(ver * 10) % 10
        esrc, edst, ewgt, ndfeat = self.graphData(tver, iver)
        g = dgl.graph((torch.tensor(esrc), torch.tensor(edst)),
                        idtype=torch.int32, num_nodes = self.numNodes())
        g.ndata['feat'] = torch.tensor(ndfeat, dtype=torch.float32)
        if iver == 2:
            g.edata['w'] = torch.tensor(ewgt, dtype=torch.float32)
        return g

    def runcmd(self, cmd, **kargs):
        for k in kargs:
            if k.isupper() and kargs[k] != -1:
                cmd += " -{} {}".format(k, kargs[k])
            if k.islower() and kargs[k]:
                cmd += " -{}".format(k)
        return self.run(cmd)

    def read(self, file):
        return self.run("r " + file + "; st")
