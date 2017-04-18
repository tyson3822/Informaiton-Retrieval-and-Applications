#include "math.h"
#include "indri/Repository.hpp"
#include "indri/CompressedCollection.hpp"
#include "indri/LocalQueryServer.hpp"
#include "indri/ScopedLock.hpp"
#include "indri/QueryEnvironment.hpp"

#include <iostream>

//#define DEBUG if(isDEBUG) 
//bool isDEBUG = false;
std::vector<std::string> vectorQuery;

class TFScore
{
public:
  std::string term;
  int document;
  long double score;
};

class IDFScore
{
public:
  std::string term;
  long double score;
};

class TF_IDFScore
{
public:
  std::string term;
  int document;
  long double score;
};

class SimilarityScore
{
public:
  int document;
  double dotProduct;
  double queryAbs;
  double docAbs;
  double simScore;
};

long double calculateTFScore(double termCount, double documentCount){
  return termCount / documentCount;
}

long double calculateIDFScore(double totalDocumentCount, double termAppearCount){
  return 1 + log(totalDocumentCount / termAppearCount);
}

long double calculateTFIDFScore(double TFScore, double IDFScore){
  return TFScore * IDFScore;
}

double calculateQueryTFScore(std::vector<string> vectorQuery, std::string query){
  int queryCount = 0;
  for(int queryIndex =0; queryIndex < vectorQuery.size(); queryIndex++)
    if(vectorQuery[queryIndex] == query) 
      queryCount++;

  return ((double)queryCount / (double)vectorQuery.size()); 
}

void calculateConsineSimilarityScore(indri::collection::Repository& r){
  //to temp the query score
  double queryTFScore, queryIDFScore, queryTFIDFScore;

  //to save the all of similarity score
  std:: vector<SimilarityScore> vectorSimScore; 

  //to temp the score
  TFScore tempTF;
  IDFScore tempIDF;
  TF_IDFScore tempTF_IDF;
  SimilarityScore tempSimScore;

  indri::server::LocalQueryServer local(r);

  indri::collection::Repository::index_state state = r.indexes();
  indri::index::Index* index = (*state)[0];
  indri::index::DocListFileIterator* iter = index->docListFileIterator();
  iter->startIteration();

  //run all of the term in the processed data
  while( !iter->finished() ) {
    indri::index::DocListFileIterator::DocListData* entry = iter->currentEntry();
    indri::index::TermData* termData = entry->termData;

    //start the term iterator
    entry->iterator->startIteration();

    //to get term idf score
    tempIDF.score = calculateIDFScore(index->documentCount(), termData->corpus.documentCount);

    //run all of the docs witch exist term
    while( !entry->iterator->finished() ) {
      //to run all query term
      for(int queryIndex = 0; queryIndex < vectorQuery.size(); queryIndex++){
        //if  current term is equal the query term
        if((termData->term == vectorQuery[queryIndex])){
          indri::index::DocListIterator::DocumentData* doc = entry->iterator->currentEntry();

          //to get query tfidf score
          queryTFScore = calculateQueryTFScore(vectorQuery, vectorQuery[queryIndex]);
          queryIDFScore = 1;
          queryTFIDFScore = queryTFScore * queryIDFScore;

          //to get tf score
          tempTF.score = calculateTFScore(doc->positions.size(), local.documentLength(doc->document ));

          //to get TFIDF score
          tempTF_IDF.score = calculateTFIDFScore(tempTF.score , tempIDF.score);

          //to get similarity score is exist or not
          bool isDocSimExist = false;
          for(int simIndex = 0;simIndex < vectorSimScore.size(); simIndex++){
            if(vectorSimScore[simIndex].document == tempTF_IDF.document){
              isDocSimExist = true;
            }//end if
          }//end for

          //if not exist, then create one
          if (!isDocSimExist){
            tempSimScore.document = doc -> document;
            tempSimScore.dotProduct = 0;
            tempSimScore.queryAbs = 0;
            tempSimScore.docAbs = 0;
            tempSimScore.simScore =0;
            vectorSimScore.push_back(tempSimScore);
          }//end if

          //calculate similarity score
          for(int simIndex = 0;simIndex < vectorSimScore.size(); simIndex++){
            if(vectorSimScore[simIndex].document == doc->document){
              vectorSimScore[simIndex].dotProduct += (tempTF_IDF.score * queryTFIDFScore);
              vectorSimScore[simIndex].queryAbs += (queryTFIDFScore * queryTFIDFScore);
              vectorSimScore[simIndex].docAbs += (tempTF_IDF.score * tempTF_IDF.score);
              vectorSimScore[simIndex].simScore = ( vectorSimScore[simIndex].dotProduct / ( sqrt(vectorSimScore[simIndex].queryAbs) * sqrt(vectorSimScore[simIndex].docAbs) ));
            }//end if
          }//end for

        }//end if
      }//end for

      //to next doc
      entry->iterator->nextEntry();

    }//end while 

    //to next term
    iter->nextEntry();

  }//end while

  //print all query term
  cout << "-Query : '";
  for(int index = 0; index < vectorQuery.size(); index++)
    cout << vectorQuery[index] << " " ;
  cout << "'" << endl;

  //print all similarity score result
  cout <<  "-Result : " << endl;
  for(int simIndex = 0; simIndex < vectorSimScore.size(); simIndex++){
    cout  << "  " << vectorSimScore[simIndex].document << "  " <<vectorSimScore[simIndex].simScore << "." << endl; 
  }

  delete iter;
}

void usage() {
  std::cout << "IR-homework <repository> <command> [ <argument> ]*" << std::endl;
  std::cout << "These commands retrieve data from the repository: " << std::endl;
  std::cout << "    Command              Argument       Description" << std::endl;
  std::cout << "    similarity(sim)     Term text       Print similarity score list from query." << std::endl;
}

#define REQUIRE_ARGS(n) { if( argc < n ) { usage(); return -1; } }

int main( int argc, char** argv ) {
  
  // std::string DEBUGCommand = argv[1];
  // if(DEBUGCommand == "DEBUG")isDEBUG = true;

  try {
    REQUIRE_ARGS(4);

    indri::collection::Repository r;
    std::string repName = argv[1];
    std::string command = argv[2];

    r.openRead( repName );

    if( command == "sim" || command == "similarity"){
      for(int index = 3; index < argc; index++)
        vectorQuery.push_back(argv[index]);
      calculateConsineSimilarityScore(r);
    } else {
      r.close();
      usage();
      return -1;
    }

    r.close();
    return 0;
  } catch( lemur::api::Exception& e ) {
    LEMUR_ABORT(e);
  }
}