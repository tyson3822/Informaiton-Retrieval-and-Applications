/*==========================================================================
 * Copyright (c) 2004 University of Massachusetts.  All Rights Reserved.
 *
 * Use of the Lemur Toolkit for Language Modeling and Information Retrieval
 * is subject to the terms of the software license set forth in the LICENSE
 * file included with this software, and also available at
 * http://www.lemurproject.org/license.html
 *
 *==========================================================================
*/

//
// dumpindex
//
// 13 September 2004 -- tds
//

#define DEBUG if(isDEBUG) 

#include "math.h"
#include "indri/Repository.hpp"
#include "indri/CompressedCollection.hpp"
#include "indri/LocalQueryServer.hpp"
#include "indri/ScopedLock.hpp"
#include "indri/QueryEnvironment.hpp"

#include <iostream>

bool isDEBUG = false;
std::vector<std::string> vectorQuery;

void print_expression_count( const std::string& indexName, const std::string& expression ) {
  indri::api::QueryEnvironment env;

  // compute the expression list using the QueryEnvironment API
  env.addIndex( indexName );
  double result = env.expressionCount( expression );
  env.close();

  std::cout << expression << ":" << result << std::endl;
}

void print_expression_list( const std::string& indexName, const std::string& expression ) {
  indri::api::QueryEnvironment env;

  // compute the expression list using the QueryEnvironment API
  env.addIndex( indexName );
  std::vector<indri::api::ScoredExtentResult> result = env.expressionList( expression );


  std::cout << expression << " " << env.termCount() << " " 
            << env.documentCount() << std::endl;

  env.close();
  // now, print the results in the format:
  // documentID weight begin end
  for( size_t i=0; i<result.size(); i++ ) {
    std::cout << result[i].document
              << " " 
              << result[i].score
              << " " 
              << result[i].begin
              << " " 
              << result[i].end
              << std::endl;
  }
}

//
// Attempts to validate the index.  Right now it only checks
// TermLists, but may do more in the future.
//

void validate( indri::collection::Repository& r ) {
  indri::collection::Repository::index_state state = r.indexes();
  indri::index::Index* index = (*state)[0];

  indri::index::TermListFileIterator* iter = index->termListFileIterator();
  int document = 1;
  iter->startIteration();

  while( !iter->finished() ) {
    indri::index::TermList* list = iter->currentEntry();
    
    if( list->terms().size() != index->documentLength( document ) ) {
      std::cout << "Document " << document << " length mismatch" << std::endl;
    }

    std::cout << document << std::endl;
    const indri::index::TermList* flist = index->termList( document );

    if( flist->terms().size() != list->terms().size() ) {
      std::cout << "Fetched version of term list is different for " << document << std::endl;
    }
    delete flist;

    document++;
    iter->nextEntry();
  }

  if( (document-1) != index->documentCount() ) {
    std::cout << "Document count (" << index->documentCount() << ") does not match term list count " << (document-1) << std::endl;
  }

  delete iter;
}

//
// Print the whole inverted file.  Each term entry starts with 
// a term statistics header (term, termCount, documentCount)
// followed by indented rows (one per document) of the form:
// (document, numPositions, pos1, pos2, ... posN ).
//

void print_invfile( indri::collection::Repository& r ) {
  indri::collection::Repository::index_state state = r.indexes();

  indri::index::Index* index = (*state)[0];
  indri::index::DocListFileIterator* iter = index->docListFileIterator();
  iter->startIteration();
  std::cout << index->termCount() << " " << index->documentCount() << std::endl;

  while( !iter->finished() ) {
    indri::index::DocListFileIterator::DocListData* entry = iter->currentEntry();
    indri::index::TermData* termData = entry->termData;
 
    entry->iterator->startIteration();

    std::cout << termData->term << " "
              << termData->corpus.totalCount << " "
              << termData->corpus.documentCount <<  std::endl;

    while( !entry->iterator->finished() ) {
      indri::index::DocListIterator::DocumentData* doc = entry->iterator->currentEntry();

      std::cout << "\t" << doc->document << " " << doc->positions.size();
      for( size_t i=0; i<doc->positions.size(); i++ ) {
        std::cout << " " << doc->positions[i];
      }
      std::cout << std::endl;

      entry->iterator->nextEntry();
    }

    iter->nextEntry();
  }

  delete iter;
}

// 
// Prints the vocabulary of the index, including term statistics.
//

void print_vocabulary( indri::collection::Repository& r ) {
  indri::collection::Repository::index_state state = r.indexes();

  indri::index::Index* index = (*state)[0];
  indri::index::VocabularyIterator* iter = index->vocabularyIterator();

  iter->startIteration();
  std::cout << "TOTAL" << " " << index->termCount() << " " << index->documentCount() << std::endl;

  while( !iter->finished() ) {
    indri::index::DiskTermData* entry = iter->currentEntry();
    indri::index::TermData* termData = entry->termData;

    std::cout << termData->term << " "
              << termData->corpus.totalCount << " "
              << termData->corpus.documentCount <<  std::endl;

    iter->nextEntry();
  }

  delete iter;
}

void print_field_positions( indri::collection::Repository& r, const std::string& fieldString ) {
  indri::server::LocalQueryServer local(r);

  UINT64 totalCount = local.termCount();

  std::cout << fieldString << std::endl;

  indri::collection::Repository::index_state state = r.indexes();

  for( size_t i=0; i<state->size(); i++ ) {
    indri::index::Index* index = (*state)[i];
    indri::thread::ScopedLock( index->iteratorLock() );

    indri::index::DocExtentListIterator* iter = index->fieldListIterator( fieldString );
    if (iter == NULL) continue;
    
    iter->startIteration();

    int doc = 0;
    indri::index::DocExtentListIterator::DocumentExtentData* entry;

    for( iter->startIteration(); iter->finished() == false; iter->nextEntry() ) {
      entry = iter->currentEntry();

      std::cout << entry->document << " "
                << entry->extents.size() << " "
                << index->documentLength( entry->document ) << " ";

      size_t count = entry->extents.size();

      for( size_t i=0; i<count; i++ ) {
        std::cout << " ( " << entry->extents[i].begin << ", " << entry->extents[i].end;
        if( entry->numbers.size() ) {
          std::cout << ", " << entry->numbers[i];
        }
        std::cout << " ) ";
      }

      std::cout << std::endl;
    }

    delete iter;
  }
}

void print_term_positions( indri::collection::Repository& r, const std::string& termString ) {
  std::string stem = r.processTerm( termString );
  indri::server::LocalQueryServer local(r);

  UINT64 totalCount = local.termCount();
  UINT64 termCount = local.termCount( termString );

  std::cout << termString << " "
            << stem << " "
            << termCount << " " 
            << totalCount << " " << std::endl;

  indri::collection::Repository::index_state state = r.indexes();

  for( size_t i=0; i<state->size(); i++ ) {
    indri::index::Index* index = (*state)[i];
    indri::thread::ScopedLock( index->iteratorLock() );

    indri::index::DocListIterator* iter = index->docListIterator( stem );
    if (iter == NULL) continue;
    
    iter->startIteration();

    int doc = 0;
    indri::index::DocListIterator::DocumentData* entry;

    for( iter->startIteration(); iter->finished() == false; iter->nextEntry() ) {
      entry = (indri::index::DocListIterator::DocumentData*) iter->currentEntry();

      std::cout << entry->document << " "
                << entry->positions.size() << " "
                << index->documentLength( entry->document ) << " ";

      size_t count = entry->positions.size();

      for( size_t i=0; i<count; i++ ) {
        std::cout << entry->positions[i] << " ";
      }

      std::cout << std::endl;
    }

    delete iter;
  }
}

void print_term_counts( indri::collection::Repository& r, const std::string& termString ) {
  std::string stem = r.processTerm( termString );
  indri::server::LocalQueryServer local(r);

  UINT64 totalCount = local.termCount();
  UINT64 termCount = local.termCount( termString );

  std::cout << termString << " "
            << stem << " "
            << termCount << " " 
            << totalCount << " " << std::endl;

  indri::collection::Repository::index_state state = r.indexes();

  for( size_t i=0; i<state->size(); i++ ) {
    indri::index::Index* index = (*state)[i];
    indri::thread::ScopedLock( index->iteratorLock() );

    indri::index::DocListIterator* iter = index->docListIterator( stem );
    if (iter == NULL) continue;

    iter->startIteration();

    int doc = 0;
    indri::index::DocListIterator::DocumentData* entry;

    for( iter->startIteration(); iter->finished() == false; iter->nextEntry() ) {
      entry = iter->currentEntry();

      std::cout << entry->document << " "
                << entry->positions.size() << " "
                << index->documentLength( entry->document ) << std::endl;
    }

    delete iter;
  }
}

void print_document_name( indri::collection::Repository& r, const char* number ) {
  indri::collection::CompressedCollection* collection = r.collection();
  //  std::string documentName = collection->retrieveMetadatum( atoi( number ), "docid" );
  std::string documentName = collection->retrieveMetadatum( atoi( number ), "docno" );
  std::cout << documentName << std::endl;
}

void print_document_text( indri::collection::Repository& r, const char* number ) {
  int documentID = atoi( number );
  indri::collection::CompressedCollection* collection = r.collection();
  indri::api::ParsedDocument* document = collection->retrieve( documentID );

  std::cout << document->text << std::endl;
  //std::cout << document->textLength << std::endl;
  delete document;
}

void print_document_data( indri::collection::Repository& r, const char* number ) {
  int documentID = atoi( number );
  indri::collection::CompressedCollection* collection = r.collection();
  indri::api::ParsedDocument* document = collection->retrieve( documentID );

  std::cout << std::endl << "--- Metadata ---" << std::endl << std::endl;

  for( size_t i=0; i<document->metadata.size(); i++ ) {
    if( document->metadata[i].key[0] == '#' )
      continue;

    std::cout << document->metadata[i].key << ": "
              << (const char*) document->metadata[i].value
              << std::endl;
  }

  std::cout << std::endl << "--- Positions ---" << std::endl << std::endl;

  for( size_t i=0; i<document->positions.size(); i++ ) {
    std::cout << i << " "
              << document->positions[i].begin << " "
              << document->positions[i].end << std::endl;

  }

  std::cout << std::endl << "--- Tags ---" << std::endl << std::endl;

  for( size_t i=0; i<document->tags.size(); i++ ) {
    std::cout << i << " "
              << document->tags[i]->name << " " 
              << document->tags[i]->begin << " "
              << document->tags[i]->end << " " 
              << document->tags[i]->number << std::endl;
  }

  std::cout << std::endl << "--- Text ---" << std::endl << std::endl;
  std::cout << document->text << std::endl;

  std::cout << std::endl << "--- Content ---" << std::endl << std::endl;
  std::cout << document->getContent() << std::endl;

  delete document;
}

void print_document_vector( indri::collection::Repository& r, const char* number ) {
  indri::server::LocalQueryServer local(r);
  lemur::api::DOCID_T documentID = atoi( number );

  std::vector<lemur::api::DOCID_T> documentIDs;
  documentIDs.push_back(documentID);

  indri::server::QueryServerVectorsResponse* response = local.documentVectors( documentIDs );
  
  if( response->getResults().size() ) {
    indri::api::DocumentVector* docVector = response->getResults()[0];
  
    std::cout << "--- Fields ---" << std::endl;

    for( size_t i=0; i<docVector->fields().size(); i++ ) {
      const indri::api::DocumentVector::Field& field = docVector->fields()[i];
      std::cout << field.name << " " << field.begin << " " << field.end << " " << field.number << std::endl;
    }

    std::cout << "--- Terms ---" << std::endl;

    for( size_t i=0; i<docVector->positions().size(); i++ ) {
      int position = docVector->positions()[i];
      const std::string& stem = docVector->stems()[position];

      std::cout << i << " " << position << " " << stem << std::endl;
    }

    delete docVector;
  }

  delete response;
}

void print_document_id( indri::collection::Repository& r, const char* an, const char* av ) {
  indri::collection::CompressedCollection* collection = r.collection();
  std::string attributeName = an;
  std::string attributeValue = av;
  std::vector<lemur::api::DOCID_T> documentIDs;

  documentIDs = collection->retrieveIDByMetadatum( attributeName, attributeValue );

  for( size_t i=0; i<documentIDs.size(); i++ ) {
    std::cout << documentIDs[i] << std::endl;
  }
}

void print_repository_stats( indri::collection::Repository& r ) {
  indri::server::LocalQueryServer local(r);
  UINT64 termCount = local.termCount();
  UINT64 docCount = local.documentCount();
  std::vector<std::string> fields = local.fieldList();
  indri::collection::Repository::index_state state = r.indexes();
  UINT64 uniqueTermCount = 0;
  for( size_t i=0; i<state->size(); i++ ) {
    indri::index::Index* index = (*state)[i];
    uniqueTermCount += index->uniqueTermCount();
  }
  std::cout << "Repository statistics:\n"
            << "documents:\t" << docCount << "\n"
            << "unique terms:\t" << uniqueTermCount    << "\n"
            << "total terms:\t" << termCount    << "\n"
            << "fields:\t\t";
  for( size_t i=0; i<fields.size(); i++ ) {
    std::cout << fields[i] << " ";
  }
  std::cout << std::endl;
}

void merge_repositories( const std::string& outputPath, int argc, char** argv ) {
  std::vector<std::string> inputs;

  for( int i=3; i<argc; i++ ) {
    inputs.push_back( argv[i] );
  }

  indri::collection::Repository::merge( outputPath, inputs );
}

void compact_repository( const std::string& repositoryPath ) {
  indri::collection::Repository r;
  r.open( repositoryPath );
  r.compact();
  r.close();
}

void delete_document( const std::string& repositoryPath, const char* stringDocumentID ) {
  lemur::api::DOCID_T documentID = (lemur::api::DOCID_T) string_to_i64( stringDocumentID );
  indri::collection::Repository r;
  r.open( repositoryPath );
  r.deleteDocument( documentID );
  r.close();
}

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

// void print_tfidf_score(indri::collection::Repository& r ){

//   TFScore tempTF;
//   IDFScore tempIDF;
//   TF_IDFScore tempTF_IDF;

//   indri::collection::Repository::index_state state = r.indexes();

//   indri::index::Index* index = (*state)[0];
//   indri::index::DocListFileIterator* iter = index->docListFileIterator();
//   iter->startIteration();

//   indri::server::LocalQueryServer local(r);

//   while( !iter->finished() ) {
//     indri::index::DocListFileIterator::DocListData* entry = iter->currentEntry();
//     indri::index::TermData* termData = entry->termData;

//     entry->iterator->startIteration();

//     //to get idf score
//     tempIDF.term = termData -> term;
//     tempIDF.score = calculateIDFScore(index->documentCount(), termData->corpus.documentCount);

//     while( !entry->iterator->finished() ) {
//       indri::index::DocListIterator::DocumentData* doc = entry->iterator->currentEntry();
        
//       //to get tf score
//       tempTF.term = termData -> term;
//       tempTF.document = doc -> document;
//       tempTF.score = calculateTFScore(doc->positions.size(), local.documentLength(doc->document ));

//       //to get TFIDF score
//       tempTF_IDF.term = termData -> term;
//       tempTF_IDF.document = doc -> document;
//       tempTF_IDF.score = calculateTFIDFScore(tempTF.score , tempIDF.score);
//       DEBUG cout << tempTF_IDF.term << " " 
//                     << tempTF_IDF.document <<  " " 
//                     <<  tempTF_IDF.score << endl;

//       entry->iterator->nextEntry();
//     }

//     iter->nextEntry();
//   }

//   delete iter;
// }

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
  std::cout << "dumpindex <repository> <command> [ <argument> ]*" << std::endl;
  std::cout << "These commands retrieve data from the repository: " << std::endl;
  std::cout << "    Command              Argument       Description" << std::endl;
  std::cout << "    term (t)             Term text      Print inverted list for a term" << std::endl;
  std::cout << "    termpositions (tp)   Term text      Print inverted list for a term, with positions" << std::endl;
  std::cout << "    fieldpositions (fp)  Field name     Print inverted list for a field, with positions" << std::endl;
  std::cout << "    expressionlist (e)   Expression     Print inverted list for an Indri expression, with positions" << std::endl;
  std::cout << "    xcount (x)           Expression     Print count of occurrences of an Indri expression" << std::endl;
  std::cout << "    documentid (di)      Field, Value   Print the document IDs of documents having a metadata field matching this value" << std::endl;
  std::cout << "    documentname (dn)    Document ID    Print the text representation of a document ID" << std::endl;
  std::cout << "    documenttext (dt)    Document ID    Print the text of a document" << std::endl;
  std::cout << "    documentdata (dd)    Document ID    Print the full representation of a document" << std::endl;
  std::cout << "    documentvector (dv)  Document ID    Print the document vector of a document" << std::endl;
  std::cout << "    invlist (il)         None           Print the contents of all inverted lists" << std::endl;
  std::cout << "    vocabulary (v)       None           Print the vocabulary of the index" << std::endl;
  std::cout << "    stats (s)                           Print statistics for the Repository" << std::endl;
  std::cout << "These commands change the data inside the repository:" << std::endl;
  std::cout << "    compact (c)          None           Compact the repository, releasing space used by deleted documents." << std::endl;
  std::cout << "    delete (del)         Document ID    Delete the specified document from the repository." << std::endl;
  std::cout << "    merge (m)            Input indexes  Merges a list of Indri repositories together into one repository." << std::endl;
}

#define REQUIRE_ARGS(n) { if( argc < n ) { usage(); return -1; } }

int main( int argc, char** argv ) {
  
  std::string DEBUGCommand = argv[1];
  if(DEBUGCommand == "DEBUG")isDEBUG = true;

  try {
    REQUIRE_ARGS(4);

    indri::collection::Repository r;
    std::string repName = argv[2];
    std::string command = argv[3];

    if( command == "c" || command == "compact" ) {
      REQUIRE_ARGS(4);
      compact_repository( repName );
    } else if( command == "del" || command == "delete" ) {
      REQUIRE_ARGS(5);
      delete_document( repName, argv[4] );
    } else if( command == "m" || command == "merge" ) {
      REQUIRE_ARGS(5);
      merge_repositories( repName, argc, argv );
    } else {
      r.openRead( repName );

      if( command == "t" || command == "term" ) {
        REQUIRE_ARGS(5);
        std::string term = argv[4];
        print_term_counts( r, term );
      } else if( command == "tp" || command == "termpositions" ) { 
        REQUIRE_ARGS(5);
        std::string term = argv[4];
        print_term_positions( r, term );
      } else if( command == "fp" || command == "fieldpositions" ) { 
        REQUIRE_ARGS(5);
        std::string field = argv[4];
        print_field_positions( r, field );
      } else if( command == "e" || command == "expression" ) {
        REQUIRE_ARGS(5);
        std::string expression = argv[4];
        print_expression_list( repName, expression );
      } else if( command == "x" || command == "xcount" ) {
        REQUIRE_ARGS(5);
        std::string expression = argv[4];
        print_expression_count( repName, expression );
      } else if( command == "dn" || command == "documentname" ) {
        REQUIRE_ARGS(5);
        print_document_name( r, argv[4] );
      } else if( command == "dt" || command == "documenttext" ) {
        REQUIRE_ARGS(5);
        print_document_text( r, argv[4] );
      } else if( command == "dd" || command == "documentdata" ) {
        REQUIRE_ARGS(5);
        print_document_data( r, argv[4] );
      } else if( command == "dv" || command == "documentvector" ) {
        REQUIRE_ARGS(5);
        print_document_vector( r, argv[4] );
      } else if( command == "di" || command == "documentid" ) {
        REQUIRE_ARGS(6);
        print_document_id( r, argv[4], argv[5] );
      } else if( command == "il" || command == "invlist" ) {
        REQUIRE_ARGS(4);
        print_invfile( r );
      } else if( command == "v" || command == "vocabulary" ) {
        REQUIRE_ARGS(4);
        print_vocabulary( r );
      } else if( command == "vtl" || command == "validate" ) {
        REQUIRE_ARGS(4);
        validate(r);
      } else if( command == "s" || command == "stats" ) {
        REQUIRE_ARGS(4);
        print_repository_stats( r );
      } else if( command == "sim"){
        for(int index = 4; index < argc; index++)
          vectorQuery.push_back(argv[index]);
        calculateConsineSimilarityScore(r);
      } else {
        r.close();
        usage();
        return -1;
      }

      r.close();
    }

    return 0;
  } catch( lemur::api::Exception& e ) {
    LEMUR_ABORT(e);
  }
}


