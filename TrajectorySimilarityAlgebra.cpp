/*
---- 
This file is part of SECONDO.

Copyright (C) 2004, University in Hagen, Department of Computer Science, 
Database Systems for New Applications.

SECONDO is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

SECONDO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SECONDO; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
----
*/


/*
Replace XXX and xxx by sufficent names and define an unique id for
a new algebra in the file Algebras/AlgebraList.i.cfg. Moreover, add a new
entry in makefile.algebras.

*/


#include "Algebra.h"
#include "NestedList.h"
#include "QueryProcessor.h"
#include "./TrajectorySimilarityAlgebra.h"
#include "TemporalAlgebra.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include "Stream.h"
#include "../Relation-C++/RelationAlgebra.h"

using namespace tsm;

extern NestedList* nl;
extern QueryProcessor *qp;

int calLcss(MPoint mp1, MPoint mp2, datetime::DateTime delta, double epsilon)
{
  int m = mp1.units.Size();
  int n = mp2.units.Size();
  if(m == 0 || n == 0){
    return 0;
  }
  int* table = new int[n];
  table = {0};
  int leftUpValue = 0;

  UPoint up1, up2;
  for(int i = 0; i < m+1; i++){
    Point p1,p2;
    Instant time1,time2;
    if(i < m)
    {
      mp1.units.Get(i, up1);
      p1 = up1.p0;
      time1 = up1.timeInterval.start;
    }
    else{
      p1 = up1.p1;
      time1 = up1.timeInterval.end;
    }
    for(int j = 0; j < n+1; j++){
      if(j < n){
        mp2.units.Get(j, up2);
        p2 = up2.p0;
        time2 = up2.timeInterval.start;
      }
      else{
        p2 = up2.p1;
        time2 = up2.timeInterval.end;
      }
      int temp = table[j];
      if(time1 - time2 < delta && p1.Distance(p2) < epsilon){
        table[j] = leftUpValue +1;
      }
      else if(table[j-1] > table[j]){
        table[j] = table[j-1];
      }
      leftUpValue = temp;
    }//for j
  }//for i
  int result = table[n];
  delete table;
  return result;
}
/****************************************************
 *
 * calculateDistance operator
 *  
 *  X.1 CalculateDistance type map
 * 
 *  LCSS(mp1, mp2, delta, epsilon)
 * 
 * **************************************************/
// ListExpr LCSSDistanceTM( ListExpr args)
// {
//   if(nl->ListLength(args) == 4){
//     ListExpr arg1 = nl->First(args);
//     ListExpr arg2 = nl->Second(args);
//     ListExpr arg3 = nl->Third(args);
//     ListExpr arg4 = nl->Fourth(args);
//     if(nl->IsEqual(arg1, MPoint::BasicType())&&
//       nl->IsEqual(arg2, MPoint::BasicType())&&
//       nl->AtomType(arg3) == IntType &&
//       nl->AtomType(arg3) == RealType){
//         return nl->SymbolAtom(CcReal::BasicType());
//     }
//   }
//   ErrorReporter::ReportError("MPoint X MPoint X Int X Real expected");
//   return (nl->SymbolAtom(Symbol::TYPEERROR()));
// }
/****************************************************
 * 
 * X.2 CalculateTrjDistance value map
 * 
 * **************************************************/
// int LCSSDistanceVM(Word* args, Word& result, int message, Word& local, Supplier s)
// {
//   MPoint* mp1Ptr = static_cast<MPoint*>(args[0].addr);
//   MPoint* mp2Ptr = static_cast<MPoint*>(args[1].addr);
//   CcInt* deltaPtr = static_cast<CcInt*>(args[2].addr);
//   CcReal* epsilonPtr = static_cast<CcReal*>(args[3].addr);

//   int mp1Length = mp1Ptr->Length();
//   int mp2Length = mp2Ptr->Length();

//   double result = calLcss(*mp1Ptr, *mp2Ptr, *deltaPtr, *epsilonPtr)/min(mp1Length, mp2Length);
//}
/****************************************************
 * 
 * now i need an new operator
 * 
 * parameter: a relation, a mpoint
 * 
 * return mpoint id which is most similar with query trajectory use lcss
 *  
 * ********************************************************/


/***************************************************
 * 
 * findLCSS() type map
 * 
 * receive 
 * 
 * **************************************************/
ListExpr TestGkTM(ListExpr args)
{
    string msg = "stream of tuple contain mpoint X mpoint X duration X real expected"; 
    if(nl->ListLength(args) != 4)
    {
        ErrorReporter::ReportError(msg);
        return nl->TypeError();
    }

    ListExpr stream = nl->First(args);
    ListExpr mpoint = nl->Second(args);
    ListExpr duration = nl->Third(args);
    ListExpr real = nl->Fourth(args);

    if(! listutils::isStream(stream)){
        ErrorReporter::ReportError(msg);
        return listutils::typeError();
    }
    if(nl->IsAtom(mpoint) && nl->IsEqual(nl->AtomType(mpoint), MPoint::BasicType()) &&
        nl->IsAtom(duration) && nl->IsEqual(nl->AtomType(duration),datetime::DateTime::BasicType()) &&
        nl->IsAtom(real) && nl->AtomType(real) == RealType){
        return NList(CcBool::BasicType()).listExpr();
    }

    ErrorReporter::ReportError(msg);
    return listutils::typeError();
}
struct FindLCSSTrjLocalInfo{
    MPoint* queryMpoint;
    int qmMostSimilarId;
    double qmMostSimilarValue;

    MPoint* queryGkMpoint;
    int qgmMostSimilarId;
    double qgmMostSimilarValue;

    FindLCSSTrjLocalInfo(){
        queryMpoint = NULL;
        qmMostSimilarId = 0;
        qmMostSimilarValue = 0;

        queryGkMpoint = NULL;
        qgmMostSimilarId = 0;
        qgmMostSimilarValue = 0;
    }
};
/*************************************************
 * 
 * for mopint 0f every tuple
 * calculate each each similarity value and restore it
 * return most similar trj 
 * *****************************************/
int TestGkVM(Word* args, Word& result, int message, Word& local, Supplier s)
{
    Tuple* tuple = NULL;
    FindLCSSTrjLocalInfo* localInfo = NULL;
    datetime::DateTime* delta = static_cast<datetime::DateTime*>(args[2].addr);
    CcReal* epsilon = static_cast<CcReal*>(args[3].addr);
    CcBool* b = static_cast<CcBool*>(result.addr);
    
    //get mpoint
    localInfo = new FindLCSSTrjLocalInfo();
    localInfo->queryMpoint = static_cast<MPoint*>(args[1].addr);
    //transfrom gk
    localInfo->queryGkMpoint = new MPoint;
    localInfo->queryMpoint->gk(39,*(localInfo->queryGkMpoint));

    Stream<Tuple> stream(args[0]);
    stream.open;

    while((tuple = stream.request()) != 0){
        MPoint tupleMp, tupleGkMp;
        tupleMp.CopyFrom((MPoint*)tuple->GetAttribute(1));
        tupleMp.gk(39,tupleGkMp);
        int mpLcssValue = calLcss(*(localInfo->queryMpoint),tupleMp, *delta, epsilon->GetValue());
        int gkmpLcssValue = calLcss(*(localInfo->queryGkMpoint), tupleGkMp, *delta, epsilon->GetValue()*10.0);
        if(mpLcssValue > localInfo->qmMostSimilarValue && 
            gkmpLcssValue > localInfo->qgmMostSimilarValue){
            //update value
            localInfo->qmMostSimilarValue = mpLcssValue;
            localInfo->qmMostSimilarId = ((CcInt*)tuple->GetAttribute(0))->GetValue();

            localInfo->qgmMostSimilarValue = gkmpLcssValue;
            localInfo->qgmMostSimilarId = localInfo->qmMostSimilarId;

            //comtinue
            tuple->DeleteIfAllowed();
            
        }
        else if(mpLcssValue <= localInfo->qmMostSimilarValue && 
            gkmpLcssValue <= localInfo->qgmMostSimilarValue){
            //comtinue

            tuple->DeleteIfAllowed();
        }
        else{
            //open a file and record it
            fstream fout("/home/chen/Jared/abnormal.log", "w");
            fout << "raw data have different similarty" << endl;
            fout << "query traject:" << endl;
            fout << "id :" << localInfo->qmMostSimilarId ;
            fout << " value:" << localInfo->qmMostSimilarValue <<endl;
            fout << "id :" << ((CcInt*)tuple->GetAttribute(0))->GetValue();
            fout << " value:" << mpLcssValue << endl;
            fout << "gk query trajectory:"<<endl;
            fout << "id :" << localInfo->qgmMostSimilarId;
            fout << " value:" << localInfo->qgmMostSimilarValue << endl;
            fout << "id :" << ((CcInt*)tuple->GetAttribute(0))->GetValue();
            fout << " value:" << gkmpLcssValue << endl;  
            fout.close();

            stream.close();
            b->Set(true, false);
            return 0;
        }   
    }
    stream.close();
    b->Set(true, true);
    return 0;
}

struct TestGkInfo : OperatorInfo {
    TestGkInfo()
    {
        name      = "testGk";
        signature = "stream X mpoint -> bool";
        syntax    = "_ testGk(_)";
        meaning   = "test gk ";
    }
};

/**
 * 
 * loadData
 * 
 */
ListExpr LoadDataTypeMap(ListExpr args)
{
    //error message;
    string msg = "string expected";

    if( nl->ListLength(args) != 1){
        ErrorReporter::ReportError(msg + " (invalid number of arguments)");
        return nl->TypeError();
    }
    ListExpr filename = nl->First(args);
    if(nl->SymbolValue(filename) != "string"){
        ErrorReporter::ReportError(msg + " (first args is not a string)");
        return listutils::typeError();
    }
    return nl->TwoElemList(
            nl->SymbolAtom("stream"),
            nl->TwoElemList(
                nl->SymbolAtom("tuple"),
                nl->ThreeElemList(
                    nl->TwoElemList(
                        nl->SymbolAtom("Id"),
                        nl->SymbolAtom("int")),
                    nl->TwoElemList(
                        nl->SymbolAtom("Datetime"),
                        nl->SymbolAtom("instant")),
                    nl->TwoElemList(
                        nl->SymbolAtom("Position"),
                        nl->SymbolAtom("point"))
                    )));
}

// LoadData local information
class LoadDataLocalInfo
{
    public:
        RecordManager *rm;
        ListExpr      resulttype;

        LoadDataLocalInfo(){
            rm = NULL;
        }
        ~LoadDataLocalInfo(){
            if(rm){
                delete rm;
            }
        }
};

// LoadData value map function
int LoadDataValueMap(Word *args, Word &result, int message, Word &local, Supplier s)
{
    LoadDataLocalInfo *localinfo;
    GPSRecord         gr;
    Tuple             *tuple = NULL;
    DateTime          *dt;

    switch(message)
    {
        case OPEN:
            {
                if(! local.addr){
                    localinfo = (LoadDataLocalInfo *)local.addr;
                    delete localinfo;
                }
                localinfo = new LoadDataLocalInfo();
                string path_name  = ((CcString*)args[0].addr)->GetValue();
                // initial the record manager
                localinfo->rm = new RecordManager();
                localinfo->rm->setName(path_name.c_str());
                cout<<"file name: "<<path_name.c_str()<<endl;
                if(! localinfo->rm->Init()){
                    cout<<"record manager initial failed!"<<endl;
                    return CANCEL;
                }
                localinfo->resulttype = nl->Second(GetTupleResultType(s));
                local.setAddr(localinfo);
                return 0;
            }
        case REQUEST:
            {
                if(NULL == local.addr){
                    return CANCEL;
                }
                localinfo = (LoadDataLocalInfo *)local.addr;
                // get next record, gps record
                if(! localinfo->rm->getNextRecord(gr)){
                    cout<<"Warning: end to read record!"<<endl;
                    return CANCEL;
                }
                dt = new DateTime(instanttype);
                dt->Set(gr.yy, gr.mm, gr.dd, gr.h, gr.m, gr.s);
                tuple = new Tuple(localinfo->resulttype);
                tuple->PutAttribute(0, new CcInt(true, gr.Oid));
                tuple->PutAttribute(1, dt);
                tuple->PutAttribute(2, new Point(true, gr.lo, gr.la));
                result.setAddr(tuple);
                return YIELD;
            }
        case CLOSE:
            {
                localinfo = (LoadDataLocalInfo *)local.addr;
                delete localinfo;
                local.setAddr(Address(0));
                return 0;
            }
            return 0;
    }
    return 0;
}


// operator info of LoadData
struct LoadDataInfo : OperatorInfo {
    LoadDataInfo()
    {
        name      = "loaddata";
        signature = "string -> stream(tuple(Id, Datetime, Position))";
        syntax    = "loaddata ( _ )";
        meaning   = "load moving object trajectory data from files";
    }
};


ListExpr LoadDataFromDirTypeMap(ListExpr args)
{
    //error message;
    string msg = "string x string expected";

    if( nl->ListLength(args) != 2){
        ErrorReporter::ReportError(msg + " (invalid number of arguments)");
        return nl->TypeError();
    }
    ListExpr filename = nl->First(args);
    ListExpr extension = nl->Second(args);
    if(nl->SymbolValue(filename) != "string"){
        ErrorReporter::ReportError(msg + " (first args is not a string)");
        return listutils::typeError();
    }
    if(nl->SymbolValue(extension) != "string"){
        ErrorReporter::ReportError(msg + " (second args is not a string)");
        return listutils::typeError();
    }
    return nl->TwoElemList(
            nl->SymbolAtom("stream"),
            nl->TwoElemList(
                nl->SymbolAtom("tuple"),
                nl->ThreeElemList(
                    nl->TwoElemList(
                        nl->SymbolAtom("Id"),
                        nl->SymbolAtom("int")),
                    nl->TwoElemList(
                        nl->SymbolAtom("Datetime"),
                        nl->SymbolAtom("instant")),
                    nl->TwoElemList(
                        nl->SymbolAtom("Position"),
                        nl->SymbolAtom("point"))
                    )));
}

// LoadDataFromDir local information
class LoadDataFromDirLocalInfo
{
    public:
        RecordManager *rm;
        ListExpr      resulttype;

        LoadDataFromDirLocalInfo(){
            rm = NULL;
        }
        ~LoadDataFromDirLocalInfo(){
            if(rm){
                delete rm;
            }
        }
};

// LoadDataFromDir value map function
int LoadDataFromDirValueMap(Word *args, Word &result, int message, Word &local, Supplier s)
{
    LoadDataFromDirLocalInfo *localinfo;
    GPSRecord         gr;
    Tuple             *tuple;
    DateTime          *dt;

    switch(message)
    {
        case OPEN:
            {
                if(! local.addr){
                    localinfo = (LoadDataFromDirLocalInfo *)local.addr;
                    delete localinfo;
                }
                localinfo = new LoadDataFromDirLocalInfo();
                string path_name  = ((CcString*)args[0].addr)->GetValue();
                string file_extension  = ((CcString*)args[1].addr)->GetValue();
                // initial the record manager
                localinfo->rm = new RecordManager();
                localinfo->rm->setName(path_name.c_str());
                localinfo->rm->setExtension(file_extension.c_str());
                if(! localinfo->rm->Init()){
                    cout<<"record manager initial failed!"<<endl;
                    return CANCEL;
                }
                localinfo->resulttype = nl->Second(GetTupleResultType(s));
                local.setAddr(localinfo);
                return 0;
            }
        case REQUEST:
            {
                if(NULL == local.addr){
                    return CANCEL;
                }
                localinfo = (LoadDataFromDirLocalInfo *)local.addr;
                // get next record, gps record
                if(! localinfo->rm->getNextRecord(gr)){
                    cout<<"Warning: end to read record!"<<endl;
                    return CANCEL;
                }
                dt = new DateTime(instanttype);
                dt->Set(gr.yy, gr.mm, gr.dd, gr.h, gr.m, gr.s);
                tuple = new Tuple(localinfo->resulttype);
                tuple->PutAttribute(0, new CcInt(true, gr.Oid));
                tuple->PutAttribute(1, dt);
                tuple->PutAttribute(2, new Point(true, gr.lo, gr.la));
                result.setAddr(tuple);
                return YIELD;
            }
        case CLOSE:
            {
                localinfo = (LoadDataFromDirLocalInfo *)local.addr;
                delete localinfo;
                local.setAddr(Address(0));
                return 0;
            }
            return 0;
    }
    return 0;
}

// operator info of LoadDataFromDir
struct LoadDataFromDirInfo : OperatorInfo {
    LoadDataFromDirInfo()
    {
        name      = "loaddatafromdir";
        signature = "string x string -> stream(tuple(Id, Datetime, Position))";
        syntax    = "loaddatafromdir ( _ , _ )";
        meaning   = "load moving object trajectory data from directory";
    }
};

ListExpr ConvertGPS2MPTM(ListExpr args)
{
    //error message;
    string msg = "stream of gps record (id, time, x, y) X duration X real expected";
    
    if(nl->ListLength(args) != 3)
    {
        ErrorReporter::ReportError(msg);
        return nl->TypeError();
    }
    //
    ListExpr stream = nl->First(args);
    ListExpr instant = nl->Second(args);
    ListExpr distance = nl->Third(args);

    if(! listutils::isStream(stream)){
        ErrorReporter::ReportError(msg);
        return listutils::typeError();
    }

    if( !listutils::isSymbol(instant) || !listutils::isSymbol(distance)){
        ErrorReporter::ReportError(msg);
        return listutils::typeError();
    }
    //cout << "I am type mapping function" << endl;
    // construct the result type ListExpr
    ListExpr resType = nl->TwoElemList(
            nl->SymbolAtom(Symbol::STREAM()),
            nl->TwoElemList(
                nl->SymbolAtom(Tuple::BasicType()),
                nl->TwoElemList(
                    nl->TwoElemList(
                        nl->SymbolAtom("Id"),
                        nl->SymbolAtom("int")),
                    nl->TwoElemList(
                        nl->SymbolAtom("Trip"),
                        nl->SymbolAtom("mpoint")))));
    // return the result type and attribute index
    return resType;
}
// local information
//
class GPS2MPLocalInfo
{
    private:
        MPoint          *mpoint = NULL;
        //new
        int curId;
        Instant oldtime,curtime;
        Point oldpos,  curpos;
        

        int count = 1;

        
    public:
        ListExpr        tupletype;
        int oldId = 0;
        Instant dur;
        CcReal* distance;
        Tuple* tuple = NULL;

        void parseTuple(){
            assert(tuple->GetNoAttributes() == 3);
            curId = ((CcInt*)tuple->GetAttribute(0))->GetValue();
            curtime.CopyFrom((Instant*)tuple->GetAttribute(1));
            curpos.CopyFrom((Point*)tuple->GetAttribute(2));
        }

        bool hasDifferentId(){
            return oldId != curId;
        }

        bool intervalTooLong(){
            assert(curtime >= oldtime);
            //cout << oldtime.ToString() << "  "<< curtime.ToString() << endl;
           
            return curtime - oldtime > dur;
        }

        bool distanceTooLong(){
            return oldpos.Distance(curpos) > distance->GetValue();
        }

        bool mpointIsNull()
        {
            return mpoint == NULL;
        }

        Tuple* packMPoint()
        {
            mpoint->EndBulkLoad(false); //no sort
            //mpoint->Print(cout) << endl;
            Tuple *tnew = new Tuple(tupletype);
            // construct the tuple
            //cout << "mpoint " << count << "th"<< endl;
            tnew->PutAttribute(0, new CcInt(count++));   
            tnew->PutAttribute(1, mpoint); 
            
            mpoint = NULL;  
            return tnew;
        }
        
        void updateInfo()
        {
            oldId = curId;
            oldtime = curtime;
            oldpos = curpos;
        }

        void constructUpoint()
        {
            Interval<Instant> utime(Interval<Instant>(oldtime, curtime,true,true));
            UPoint upoint(utime, oldpos, curpos);
            //upoint.Print(cout) << endl;
            assert(oldId == curId);
            if(mpointIsNull()){
                mpoint = new MPoint(0);
                mpoint->StartBulkLoad();
            }
            mpoint->Add(upoint);
            //mpoint->Print(cout)<< endl; 
        }
};

//value map function
//
int ConvertGPS2MPVM(Word* args, Word& result, int message, Word& local, Supplier s)
{
    Tuple *tuple = NULL;
    GPS2MPLocalInfo *localinfo = NULL;
    //cout << "I am value mapping function" << endl;

    switch( message )
    {
        case OPEN:
            //cout << " case open "<< endl;
            if(local.addr){
                localinfo = (GPS2MPLocalInfo*)local.addr;
                delete localinfo;
            }
            localinfo = new GPS2MPLocalInfo();
            localinfo->tupletype = nl->Second(GetTupleResultType(s));
            localinfo->dur.Equalize((DateTime*)args[1].addr);
            localinfo->distance = static_cast<CcReal*>(args[2].addr);
            
            qp->Open(args[0].addr);
            local = SetWord(localinfo);
            return 0;

        case REQUEST:
        {
            if(!local.addr){
                cout<<"local.addr is null!"<<endl;
                return CANCEL;
            }
            localinfo = (GPS2MPLocalInfo*)local.addr;
            //cout << localinfo->dur.ToString() << endl;
            //cout << localinfo->distance << endl;
            Word elem;
            qp->Request(args[0].addr, elem);
            while(qp->Received(args[0].addr)){
                localinfo->tuple = (Tuple*)elem.addr;
                localinfo->parseTuple();
                if(localinfo->oldId == 0 
                 || localinfo->hasDifferentId()
                 || localinfo->intervalTooLong()
                 || localinfo->distanceTooLong())
                {
                    if(!localinfo->mpointIsNull()){
                        tuple = localinfo->packMPoint();
                        result.setAddr(tuple);
                        localinfo->updateInfo();
                        return YIELD;
                    } 
                }
                else{                    
                    localinfo->constructUpoint();
                }

                localinfo->updateInfo();
                qp->Request(args[0].addr, elem);
            }
            if(localinfo->mpointIsNull()){
                return CANCEL;
            }
            else{
                tuple = localinfo->packMPoint();
                result.setAddr(tuple);
                return YIELD;
            }
        }
        case CLOSE:
            if(local.addr){
                localinfo = (GPS2MPLocalInfo*)local.addr;
                qp->Close(args[0].addr);
                delete localinfo;
                localinfo = NULL;
                local.addr = NULL;
            }
            return 0;
    }
    return 0;
}

//
// operator info
struct ConvertGPS2MPInfo : OperatorInfo {
    ConvertGPS2MPInfo()
    {
        name      = "convertGPS2MP";
        signature = "stream(tuple((x1,t1)..(xn, tn)) x duration x Real-> stream(tuple((x1,t1)..(xn, tn)))";
        syntax    = "_ convertGPS2MP [ _ , _ ]";
        meaning   = "convert GPSdata to MPoints";
    }
};
/*
   Type Mapping for ~gkproject~

*/
ListExpr GKProjectTM(ListExpr args){
    int len = nl->ListLength(args);
    if( len != 1 ){
        return listutils::typeError(" one arguments expected");
    }
    string err = "point expected";
    ListExpr arg = nl->First(args);
    if(!listutils::isSymbol(arg)){
        return listutils::typeError(err);
    }
    string t = nl->SymbolValue(arg);
    if(t != Point::BasicType()){
        return listutils::typeError(err);
    }
    return nl->SymbolAtom(t); // Zone provided by user
}

int MyGK(Point &src, Point &result)   // src.x : longitude, src.y : latitude
{
    WGSGK mygk;
    mygk.setMeridian((int)(src.GetX()+1.5)/3.0);
    mygk.enableWGS(false);
    if(! mygk.project(src, result)){
        cout<<"ERROR: in mygk!"<<endl;
        return -1;
    }
    return 1;
}

/*
   Value Mapping for ~gkproject~

*/
int GKProjectVM(Word* args, Word& result, int message, Word& local, Supplier s){
    result = qp->ResultStorage(s);
    Point* p = static_cast<Point*>(args[0].addr);
    Point* res = static_cast<Point*>(result.addr);
    MyGK(*p, *res);
    return 0;
}
//
// operator info
struct GKProjectInfo : OperatorInfo {
    GKProjectInfo()
    {
        name      = "gkproject";
        signature = "point -> point";
        syntax    = "gkproject(  _ )";
        meaning   = "gk projection of point";
    }
};

//XXXAlgebra xxxAlgebra;
class TrajectorySimilarityAlgebra : public Algebra
{
  public:
    TrajectorySimilarityAlgebra() : Algebra()
    {

      //AddOperator(InsertTrajectoryInfo(), InsertTrajectoryVM, InsertTrajectoryTM);
      //AddOperator(FindSimilarTrjInfo(), FindSimilarTrjVM, FindSimilarTrjTM);
      AddOperator(LoadDataInfo(), LoadDataValueMap, LoadDataTypeMap);
      AddOperator(LoadDataFromDirInfo(), LoadDataFromDirValueMap, LoadDataFromDirTypeMap);
      AddOperator(ConvertGPS2MPInfo(), ConvertGPS2MPVM, ConvertGPS2MPTM);
      AddOperator(GKProjectInfo(), GKProjectVM, GKProjectTM);
    }
    ~TrajectorySimilarityAlgebra() {}
};



/*

5 Initialization

*/

extern "C"
Algebra*
InitializeTrajectorySimilarityAlgebra(NestedList *nlRef, QueryProcessor *qpRef)
{
  nl = nlRef;
  qp = qpRef;
  return (new TrajectorySimilarityAlgebra());
}


