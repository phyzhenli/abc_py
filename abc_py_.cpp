#include <iostream>
#include <boost/format.hpp>
#include <boost/python/numpy.hpp>
namespace py = boost::python;
namespace np = boost::python::numpy;

#include <chrono>
using namespace std::chrono;

#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "map/scl/sclSize.h"

#if defined(ABC_NAMESPACE)
namespace ABC_NAMESPACE
{
#elif defined(__cplusplus)
extern "C"
{
#endif
// procedures to start and stop the ABC framework
// (should be called before and after the ABC procedures are called)
void   Abc_Start();
void   Abc_Stop();
// procedures to get the ABC framework and execute commands in it
Abc_Frame_t_ * Abc_FrameGetGlobalFrame();
int    Cmd_CommandExecute( Abc_Frame_t_ * pAbc, const char * sCommand );
#if defined(ABC_NAMESPACE)
}
using namespace ABC_NAMESPACE;
#elif defined(__cplusplus)
}
#endif

#define AssertMsg(cond, msg, ret) do { if(!(cond)) { std::cerr << (msg) << std::endl; return ret; } } while(0)

/// @class ABC_PY::AbcInterface
/// @brief the interface to ABC
class AbcInterface {
    Abc_Frame_t_ * _pAbc = nullptr; ///< The pointer to the ABC framework
public:
    /// @brief start the ABC framework
    AbcInterface();
    /// @brief end the ABC framework
    ~AbcInterface();
    /// @brief run an ABC command
    py::tuple run(const char* cmd);
    /// @brief get the design stats from ABC
    py::tuple ntkStats();
    /// @brief get the design stats after Standard-cell library mapping
    py::tuple sclStats();
    /// @brief get the design stats of the AIG saved in verilog format (for DRiLLS)
    py::tuple yosysStats();
    /// @return the number of total nodes
    int numNodes() const { return Abc_NtkObjNum(_pAbc->pNtkCur); }
    /// @brief get the graph data
    py::tuple graphData(int tver, int iver);
};

AbcInterface::AbcInterface() {
    Abc_Start();              // start the ABC framework
    _pAbc = Abc_FrameGetGlobalFrame();
    Abc_UtilsSource(_pAbc);   // source the resource file "abc.rc"
}

AbcInterface::~AbcInterface() {
    Abc_Stop();    // stop the ABC framework
}

py::tuple AbcInterface::run(const char* cmd) {
    auto start = steady_clock::now();
    auto status = Cmd_CommandExecute(_pAbc, cmd);
    duration<double> elapsed = steady_clock::now() - start;
    if (status)
        std::cerr << (boost::format("Cannot execute command \"%s\".") % cmd) << std::endl;
    return py::make_tuple(elapsed.count(), !status);
}

np::ndarray make_array(int* src, int s0, int s1 = 0) {
    auto size = s0 * sizeof(int);
    auto shape = py::tuple{};
    if (!s1) shape = py::make_tuple(s0);
    else { size *= s1; shape = py::make_tuple(s0, s1); }
    auto a = np::empty(shape, np::dtype::get_builtin<int>());
    memcpy(a.get_data(), src, size);
    return a;
}

py::tuple AbcInterface::graphData(int tver, int iver) {
    constexpr int num_tver = 3, num_iver = 3;
    static const int typid[][num_tver] = {
        { 0, 0, 0}, // CONST1
        { 1, 1, 1}, // PI
        { 2, 2, 2}, // PO
        {-1, 3, 2}, // BI
        {-1, 4, 1}, // BO
        {-1,-1,-1}, // NET,  unused
        { 3, 6, 4}, // NODE, has the max id
        {-1, 5, 3}  // LATCH
    };
    AssertMsg(tver >= 0 && tver < num_tver, "Unexpected graph version", {});
    AssertMsg(iver >= 0 && iver < num_iver, "Unexpected graph version", {});
    Abc_Ntk_t* pNtk = _pAbc->pNtkCur;
    AssertMsg(pNtk, "No current network", {});
    AssertMsg(Abc_NtkIsStrash(pNtk), "Unexpected network type", {});
    int nfeats = typid[ABC_OBJ_NODE-1][tver] + 1;   // one-hot coding for node type
    if (iver == 0) nfeats += 2;     // ninv encoded into type
    else if (iver == 1) ++nfeats;   // ninv as a feature
    std::vector<int> esrc, edst, ewgt, ndfeat;
    auto add_edge = [&esrc, &edst, &ewgt](int src, int dst, int inv)
            { esrc.push_back(src); edst.push_back(dst); ewgt.push_back(inv ? -1 : 1); };
    int i; Abc_Obj_t* pObj;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        int ninv = 0;
        int tid  = -1;
        int tobj = Abc_ObjType(pObj) - 1;
        if (tobj >= 0 && iver < ABC_OBJ_LATCH) tid = typid[tobj][tver];
        AssertMsg(tid >= 0, boost::format("Unexpected node type %d") % tobj, {});
        if (Abc_ObjIsNode(pObj)) {
            add_edge(Abc_ObjFaninId0(pObj), i, Abc_ObjFaninC0(pObj));
            add_edge(Abc_ObjFaninId1(pObj), i, Abc_ObjFaninC1(pObj));
            ninv = Abc_ObjFaninC0(pObj) + Abc_ObjFaninC1(pObj);
            if (iver == 0) tid += ninv;
        }
        else if (Abc_ObjIsCo(pObj)) {
            int numFanin = Abc_ObjFaninNum(pObj);
            AssertMsg(numFanin == 1, boost::format("PO node has %d fanin") % numFanin, {});
            add_edge(Abc_ObjFaninId0(pObj), i, 0);
        }
        ndfeat.insert(ndfeat.end(), nfeats, 0);
        auto feat = ndfeat.end() - nfeats;
        feat[tid] = 1;
        if (iver == 1) feat[nfeats-1] = ninv;
    }
    int nedges = esrc.size();
    return py::make_tuple(make_array(esrc.data(), nedges), make_array(edst.data(), nedges),
                          make_array(ewgt.data(), nedges), make_array(ndfeat.data(), i, nfeats));
}

py::tuple AbcInterface::ntkStats() {    // (isaig, i, o, lat, nd, edge, lev)
    Abc_Ntk_t* pNtk = _pAbc->pNtkCur;
    AssertMsg(pNtk, "no current network", {});
    AssertMsg(!Abc_NtkIsNetlist(pNtk), "Unexpected network type", {});
    return py::make_tuple(Abc_NtkIsStrash(pNtk), Abc_NtkPiNum(pNtk), Abc_NtkPoNum(pNtk),
            Abc_NtkLatchNum(pNtk), Abc_NtkNodeNum(pNtk), Abc_NtkGetTotalFanins(pNtk),
            Abc_NtkIsStrash(pNtk) ? Abc_AigLevel(pNtk): Abc_NtkLevel(pNtk));
}

py::tuple AbcInterface::sclStats() {    // (area, delay)
    Abc_Ntk_t* pNtk = _pAbc->pNtkCur;
    AssertMsg(pNtk, "No current network", {});
    AssertMsg(Abc_NtkIsLogic(pNtk) && Abc_NtkHasMapping(pNtk), "Unexpected network type", {});
    AssertMsg(_pAbc->pLibScl, "No Liberty library available", {});
    pNtk = Abc_NtkDupDfs(pNtk);
    AssertMsg(pNtk, "topo failed", {});
    Abc_Ntk_t* pNtk1 = pNtk;
    if (pNtk->nBarBufs2 > 0) {
        pNtk1 = Abc_NtkDupDfsNoBarBufs(pNtk);
        Abc_NtkDelete(pNtk);
    }
    SC_Man* p = Abc_SclManStart((SC_Lib*)_pAbc->pLibScl, pNtk1, 0, 1, 0, 0);
    int fRise = 0;
    Abc_Obj_t*pPivot = Abc_SclFindCriticalCo(p, &fRise);
    p->ReportDelay = Abc_SclObjTimeOne(p, pPivot, fRise);
    auto result = py::make_tuple(Abc_SclGetTotalArea(p->pNtk), p->ReportDelay);
    Abc_SclManFree(p);
    Abc_NtkDelete(pNtk1);
    return result;
}

py::tuple AbcInterface::yosysStats() {    // (and, or, not)
    Abc_Ntk_t* pNtk = _pAbc->pNtkCur;
    AssertMsg(pNtk, "no current network", {});
    AssertMsg(Abc_NtkIsStrash(pNtk), "Unexpected network type", {});
    int n_and = 0, n_or = 0, n_not = 0;
    int i; Abc_Obj_t* pObj;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        if (!Abc_ObjIsNode(pObj)) continue;
        int ninv = Abc_ObjFaninC0(pObj) + Abc_ObjFaninC1(pObj);
        Abc_Obj_t* fanout = Abc_ObjFanout0(pObj);
        if (Abc_ObjIsCo(fanout) && Abc_ObjFaninC0(fanout))
            ++n_or, n_not += 2 - ninv;
        else
            ++n_and, n_not += ninv;
    }
    return py::make_tuple(n_and, n_or, n_not);
}

BOOST_PYTHON_MODULE(abc_py_) {
    np::initialize();

    py::class_<AbcInterface, boost::noncopyable>("AbcInterface_")
        .def("run",        &AbcInterface::run)
        .def("ntkStats",   &AbcInterface::ntkStats)
        .def("sclStats",   &AbcInterface::sclStats)
        .def("yosysStats", &AbcInterface::yosysStats)
        .def("numNodes",   &AbcInterface::numNodes)
        .def("graphData",  &AbcInterface::graphData);
}
