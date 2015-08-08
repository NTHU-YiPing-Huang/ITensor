//
// Distributed under the ITensor Library License, Version 1.2
//    (See accompanying LICENSE file.)
//
#include "itensor/util/count.h"
#include "itensor/matrix/lapack_wrap.h"
#include "itensor/tensor/contract.h"
#include "itensor/iqtensor.h"
#include "itensor/util/count.h"
#include "itensor/util/stdx.h"

namespace itensor {

using std::istream;
using std::ostream;
using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::find;
using std::pair;
using std::make_pair;
using std::move;




template<> IQTensor::
ITensorT(const IQIndex& i1) :
    is_(i1),
    scale_(1.)
    { }

template<> IQTensor::
ITensorT(const IQIndex& i1,
         const IQIndex& i2) :
    is_(i1,i2),
    scale_(1.)
    { }


template<>
IQTensor::
ITensorT(Complex val) 
    :
    scale_(1.)
    { 
    //TODO: change storage type to IQTDiag?
    if(val.imag()==0)
        store_ = newITData<ITDiag<Real>>(1,val.real());
    else
        store_ = newITData<ITDiag<Complex>>(1,val);
    }

//IQTensor::
//IQTensor(const QN& q, vector<IQIndex>&& iqinds) 
//	: 
//    is_(move(iqinds)),
//    store_(newITData<IQTReal>(is_,q)),
//    div_(q),
//    scale_(1.)
//	{ }

//IQTensor::
//IQTensor(const QN& q,
//         IQIndexSet&& iset,
//         NewData nd,
//         LogNumber scale)
//    :
//    is_(move(iset)),
//    store_(move(nd)),
//    div_(q),
//    scale_(scale)
//    {
//    }



//IQTensor&
//operator*=(IQTensor& A, const IQTensor& B)
//    {
//    if(!A || !B)
//        Error("Default constructed IQTensor in product");
//
//    if(&A == &B)
//        {
//        A = IQTensor(sqr(norm(A)));
//        return A;
//        }
//
//    auto& Lis = A.inds();
//    auto& Ris = B.inds();
//
//    auto checkDirs = 
//    [&Lis,&Ris](const IQIndex& li, const IQIndex& ri)
//        {
//        if(li.dir() == ri.dir())
//            {
//            println("-------------------------");
//            println("Left indices = \n",Lis);
//            println("-------------------------");
//            println("Right indices = \n",Ris);
//            println("-------------------------");
//            println("IQIndex from left = \n",li);
//            println("IQIndex from right = \n",ri);
//            Error("Incompatible arrow directions in IQTensor contraction");
//            }
//        };
//    Label Lind,
//          Rind;
//    computeLabels(Lis,Lis.r(),Ris,Ris.r(),Lind,Rind,checkDirs);
//
//    auto nstore = A.store();
//    auto C = doTask(Contract<IQIndex>{Lis,Lind,Ris,Rind},nstore,B.store());
//
//    auto nscale = A.scale()*B.scale();
//    if(!std::isnan(C.scalefac)) nscale *= C.scalefac;
//
//#ifdef DEBUG
//    //Check for duplicate indices
//    detail::check(C.Nis);
//#endif
//
//    A = IQTensor(C.Nis,std::move(nstore),nscale);
//
//    return A;
//    }

struct AddITensor
    {
    const QN& tdiv;
    const IQIndexSet& iqis;
    const IndexSet& is;
    const Label& block_ind;
    const Permutation& P;
    Real fac = 0;
    AddITensor(QN const& tdiv_,
               IQIndexSet const& iqis_,
               IndexSet const& is_,
               Label const& block_ind_,
               Permutation const& P_,
               Real scalefac_)
        :
        tdiv(tdiv_),
        iqis(iqis_),
        is(is_),
        block_ind(block_ind_),
        P(P_),
        fac(scalefac_)
        { }
    };

void
doTask(AddITensor & A, 
       IQTReal & d, 
       ITReal const& t)
    {
    auto ddiv = doTask(CalcDiv{A.iqis},d);
    if(ddiv != A.tdiv) Error("IQTensor+=ITensor, ITensor has incompatible QN flux/divergence");
    Range drange;
    drange.init(make_indexdim(A.iqis,A.block_ind));
    auto* dblock = getBlock(d,A.iqis,A.block_ind);

    auto dref = makeTenRef(dblock,drange);
    auto tref = makeTenRef(t.data(),A.is);
    auto add = [f=A.fac](Real& r1, Real r2) { r1 += f*r2; };
    permute(tref,A.P,dref,add);
    }


IQTensor&
operator+=(IQTensor & T, 
           ITensor const& t)
    {
    if(!t) Error("IQTensor+=ITensor: r.h.s. ITensor is default constructed");
    if(!T.inds()) Error("Calling IQTensor+= on default constructed ITensor");
    auto rank = T.r();
#ifdef DEBUG
    if(t.r() != rank) Error("Mismatched number of indices in IQTensor+=ITensor");
#endif

    Permutation P(rank);
    Label block_ind(rank);
    for(auto i : count(rank))
    for(auto I : count(rank))
        {
        auto j = findindex(T.inds()[I],t.inds()[i]);
        if(j > 0)
            {
            block_ind[I] = (j-1);
            P.setFromTo(i,I);
            break;
            }
        }

    auto tdiv = calcDiv(T.inds(),block_ind);

    if(!T.store()) 
        {
        //allocate data to add this ITensor into
        if(!isComplex(t)) T.store() = newITData<IQTReal>(T.inds(),tdiv);
        else              Error("Initializing complex IQTensor in +=ITensor not yet implemented");
        }

    Real scalefac = 1;
    if(T.scale().magnitudeLessThan(t.scale())) T.scaleTo(t.scale()); 
    else                                       scalefac = (t.scale()/T.scale()).real();

    doTask(AddITensor{tdiv,T.inds(),t.inds(),block_ind,P,scalefac},T.store(),t.store());

    return T;
    }

template<>
IQTensor& IQTensor::
dag()
    {
    is_.dag();
    doTask(Conj{},store_);
    return *this;
    }


struct ToITensor
    {
    IQIndexSet const& is;
    LogNumber const& scale;

    ToITensor(IQIndexSet const& is_,
              LogNumber const& scale_)
        :
        is(is_),
        scale(scale_)
        { }
    };

ITensor
doTask(ToITensor & T, 
       IQTReal const& d)
    {
    auto r = T.is.r();
    auto nd = ITReal(area(T.is),0);
    auto *pd = d.data();
    auto *pn = nd.data();
    vector<long> block(r,0);
    detail::GCounter C(r);
    for(auto& io : d.offsets)
        {
        inverseBlockInd(io.block,T.is,block);
        for(long j = 0; j < r; ++j)
            {
            long start = 0;
            for(long b = 0; b < block[j]; ++b)
                start += T.is[j][b].m();
            C.setRange(j,start,start+T.is[j][block[j]].m()-1);
            }
        //TODO: need to make a Range/TensoRef iterator
        //to rewrite the following code more efficiently
        for(; C.notDone(); ++C)
            {
            pn[ind(T.is,C.i)] = pd[io.offset+C.ind];
            }
        }
    auto inds = IndexSetBuilder(r);
    for(decltype(r) j = 0; j < r; ++j) inds.setExtent(j,T.is[j]);
    return ITensor(IndexSet{inds},std::move(nd),T.scale);
    }

//template<typename D>
//ITensor
//doTask(ToITensor & T, ITDiag<D> const& d)
//    {
//    auto r = T.is.r();
//    IndexSet::storage_type inds(r);
//    for(long j = 0; j < r; ++j) inds[j].ext = T.is[j];
//    return ITensor(IndexSet{std::move(inds)},ITDiag<D>{d});
//    }
//template ITensor doTask(ToITensor & T, ITDiag<Real> const& d);
//template ITensor doTask(ToITensor & T, ITDiag<Cplx> const& d);


ITensor
toITensor(IQTensor const& T)
    {
    //Handle unallocated IQTensors
    if(!T.store()) 
        {
        if(T.r()==0) return ITensor{};
        auto inds = IndexSetBuilder(T.r());
        for(decltype(T.r()) j = 0; j < T.r(); ++j) inds.setExtent(j,T.inds()[j]);
        return ITensor(IndexSet{inds});
        }
    //Main case for allocated IQTensors
    return doTask(ToITensor{T.inds(),T.scale()},T.store());
    }

QN
div(IQTensor const& T) 
    { 
    if(!T) Error("div(IQTensor) not defined for unallocated IQTensor");
    return doTask(CalcDiv{T.inds()},T.store());
    }

IQTensor
combiner(std::vector<IQIndex> inds,
         Args const& args)
    {
    if(inds.empty()) Error("No indices passed to combiner");
    auto cname = args.getString("IndexName","cmb");
    auto itype = getIndexType(args,"IndexType",inds.front().type());
    auto cr = inds.size();
    auto dir = inds.front().dir();

    struct QNm { QN q; long m = 1; };
    auto qms = vector<QNm>{};

    //set up counter over all possible blocks
    //that can be formed out of "inds"
    auto C = detail::GCounter(cr);
    for(auto j : count(cr))
        C.setRange(j,1,inds[j].nindex());

    for(; C.notDone(); ++C)
        {
        QNm qm;
        for(auto j : count(cr))
            {
            qm.q += inds[j].qn(C[j]) * inds[j].dir();
            qm.m *= inds[j].index(C[j]).m();
            }
        auto q_equals = [&qm](QNm const& x){ return x.q == qm.q; };
        auto it = stdx::find_if(qms,q_equals);
        if(it != qms.end()) it->m += qm.m;
        else                qms.push_back(qm);
        }

    //Build storage for combined IQIndex
    auto indqn = IQIndex::storage(qms.size());
    for(auto n : index(indqn))
        indqn[n] = IndexQN(Index(nameint("c",n),qms[n].m,itype),qms[n].q);

    auto newinds = stdx::reserve_vector<IQIndex>(1+inds.size());
    newinds.emplace_back(cname,std::move(indqn),dir);
    for(auto const& I : inds) newinds.push_back(dag(I));

    return IQTensor(IQIndexSet{std::move(newinds)},ITCombiner());
    }

IQIndex
findIQInd(const IQTensor& T, const Index& i)
    {
    for(const IQIndex& J : T.inds())
        if(hasindex(J,i)) return J;
    Print(T.inds());
    Print(i);
    throw ITError("Index i not found in any of T's IQIndices");
    return IQIndex{};
    }


Arrow
dir(const IQTensor& T, const IQIndex& I)
	{
    for(const IQIndex& J : T.inds())
        if(I == J) return J.dir();
    Error("dir: IQIndex not found");
    return Out;
	}

bool
isZero(const IQTensor& T, const Args& args)
    {
    Error("Not implemented");
    //if(T.empty()) return true;
    ////done with all fast checks
    //if(args.getBool("Fast",false)) return false;
    //for(const ITensor& t : T.blocks())
    //    {
    //    if(!isZero(t)) return false;
    //    }
    return true;
    }

//template<typename T>
//void
//doTask(PrintIT<IQIndex>& P, const ITDiag<T>& d)
//    {
//    constexpr auto type = std::is_same<T,Real>::value ? "Real" : "Cplx";
//    P.printInfo(d,format("Diag %s%s",type,d.allSame()?", all same":""),
//                doTask(NormNoScale{},d));
//
//    auto r = P.is.r();
//
//    if(r == 0) 
//        {
//        P.s << "  ";
//        P.printVal(P.scalefac*(d.empty() ? d.val : d.store.front()));
//        return;
//        }
//
//    if(!P.print_data) return;
//
//    for(auto i : count(d.length))
//        {
//        auto val = P.scalefac*(d.allSame() ? d.val : d.store[i]);
//        if(std::norm(val) > Global::printScale())
//            {
//            P.s << "(";
//            for(decltype(r) j = 1; j < r; ++j)
//                {
//                P.s << (1+i) << ",";
//                }
//            P.s << (1+i) << ") ";
//            P.printVal(val);
//            }
//        }
//    }
//template void doTask(PrintIT<IQIndex>& P, const ITDiag<Real>& d);
//template void doTask(PrintIT<IQIndex>& P, const ITDiag<Cplx>& d);


std::ostream&
operator<<(std::ostream& s, const IQTensor& T)
    {
	s << "/--------------IQTensor--------------\n";
    if(T.store())
        {
        s << "r=" << T.r() << " div=" << div(T) << " log(scale)=" << T.scale().logNum() << "\n";
        s << T.inds();
        //Checking whether std::ios::floatfield is set enables 
        //printing the contents of an ITensor when using the printf
        //format string %f (or another float-related format string)
        bool ff_set = (std::ios::floatfield & s.flags()) != 0;

        if(ff_set || Global::printdat())
            {
            s << "\n|-- Data -------\n";
            doTask(PrintIT<IQIndex>{s,T.scale(),T.inds(),true},T.store());
            s << "\n";
            }
        }
    else
        {
        s << "r=" << T.r() << " log(scale)=" << T.scale().logNum() << "\n";
        s << T.inds();
        s << "{Zero / Not yet allocated}\n";
        }
	s << "\\------------------------------------\n\n";
    return s;
    }


} //namespace itensor
