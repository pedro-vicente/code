#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <stdexcept>
#include <vector>
#include "nexus_util.h"
#include "node.h"
#include "node_util.h"
#include "retriever.h"
#include "string_util.h"
#include "xml_parser.h"
#include "Ptr.h"
#include "tree.hh"
#include "nxtranslate_debug.h"

// ----- start of declaring debug levels
#ifdef DEBUG3_XML_PARSER
#define DEBUG2_XML_PARSER
#endif
#ifdef DEBUG2_XML_PARSER
#define DEBUG1_XML_PARSER
#endif
// ----- end of declaring debug levels

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::invalid_argument;
using std::map;
using std::runtime_error;
using std::string;
using std::vector;

typedef vector<string> StrVector;
typedef vector<Node> NodeVector;
typedef NodeVector* NodeVectorP;
typedef Ptr<Retriever> RetrieverPtr;

static const int    GROUP_STRING_LEN  = 128;
static const uint   MAX_NODE_DEPTH    = 20;
static const string DEFAULT_MIME_TYPE = "NeXus";
static const string MIME_TYPE         = "NXS:mime_type";
static const string SOURCE            = "NXS:source";
static const string LOCATION          = "NXS:location";
static const string LINK              = "NAPIlink";
static const string TARGET            = "target";
static const string TYPE              = "type";
static const string NAME              = "name";
static const string NXROOT            = "NXroot";
static const string EXCEPTION         = "EXCEPTION";
static const string INVALID_ARGUMENT  = "INVALID ARGUMENT";
static const string RUNTIME_ERROR     = "RUNTIME ERROR";

typedef struct{
  NXhandle *handle;       // output file handle
  int status;             // status of parsing
  bool is_link;           // whether the current node is a link
  map<string,string> map; // store macros for replacement
  vector<StrVector> loc_to_source; // mapping for links
  NodeVector nodes;       // vector to current node listing
  string char_data;       // character data collected for current node
  vector<int> dims;       // dimensions of the array in current node
  vector<string> mime_types; // vector of mime_types (for nesting)
  vector<RetrieverPtr> retrievers; // vector of retrievers (for nesting)
}UserData;

// variable so the line and column numbers can be accessed
static xmlParserCtxtPtr context;

/*
 * Single place for printing out error messages generated by
 * exceptions. This does set the user_data.status to -1.
 */
static void print_error(UserData *user_data, const string &message){
  user_data->status=-1;
  cerr << message << endl;
}

/*
 * Replace the key with the correct value in the map, if it exists.
 */
static void map_string(map<string,string> &map,string &key){
  // check that the key is non-zero size
  if(key.size()<=0) return;

  typedef std::map<string,string>::const_iterator map_iter;
  map_iter it=map.find(key);
  if(it!=map.end())
    key=it->second;
}

/*
 * Remove leading and trailing whitespace from a string
 */
static string trim(const string &s){
  typedef string::size_type string_size;
  string_size start=0;
  string_size end=s.size();

  for( ; start<end ; start++ ){
    if(!isspace(s[start])) break;
  }

  end--;
  for( ; start<=end ; end-- ){
    if(!isspace(s[end])) break;
  }

  if(end<start)
    return "";
  else if(start==0 && end+1==s.size())
    return s;
  else
    return s.substr(start,end+1);
}

/*
 * If len<0 then just return the whole string
 */
static string xmlChar_to_str(const xmlChar *ch, int len){
#ifdef DEBUG3_XML_PARSER
  std::cout << "xmlChar_to_str(\"" << ch << "\"," << len << ")" << std::endl;
#endif

  string result((char *)ch);
  if( (len>0) && ((uint)len<result.size()) )
    result.erase(result.begin()+len,result.end());

  return trim(result);
}

static vector<string> xmlattr_to_strvec(const xmlChar* char_array[]){
  StrVector result;
  int i=0;
  while(true){
    if((char_array+i)==NULL || *(char_array+i)==NULL)
      break;
    else{
      string str=xmlChar_to_str(*(char_array+i),-1);
      result.push_back(str);
    }
    i++;
  }

  return result;
}
#ifdef DEBUG1_XML_PARSER
static void print_strvec(const StrVector &vec){
  // print out the string vector
  cout << "[";
  for( StrVector::const_iterator str=vec.begin() ; str!=vec.end() ; str++ ){
    cout << *str;
    if(str+1!=vec.end())
      cout << ",";
  }
  cout << "]" << endl;
}

static void print_retrievervec(const vector<RetrieverPtr> &vec){
  cout << "[";
  for(vector<RetrieverPtr>::const_iterator it=vec.begin();it!=vec.end();it++){
    cout << (*it)->toString(); // a pointer to a Ptr
    if(it+1!=vec.end())
      cout << ",";
  }
  cout << "]" << endl;
}
#endif

static Attr make_attr(const string &name, const string &value){
  // if value is empty return empty attribute (delete it from node)
  if(value.size()<=0)
    return Attr(name,NULL,0,NX_CHAR);

  // if the attribute does not start with "NX_" it is a character
  if(value.substr(0,3)!="NX_")
    return Attr(name,value.c_str(),value.size(),NX_CHAR);
  //else                                                          // REMOVE
  //std::cout << "FOUND:" << name << "|" << value << std::endl; // REMOVE

  // split the string for type and value
  static const char COLON=':';
  string::size_type loc=1;
  for( ; loc<value.size() ; loc++ )
    if(COLON==value[loc]) break;
  string my_type=value.substr(0,loc);
  string my_val=value.substr(loc+1,value.size());

  //std::cout << "TYPE=" << my_type << " VALUE=" << my_val << std::endl; //REMOVE

  // convert the string type to an integer type
  Node::NXtype int_type;
  if(my_type=="NX_CHAR")
    int_type=Node::CHAR;
  else if(my_type=="NX_FLOAT32")
    int_type=Node::FLOAT32;
  else if(my_type=="NX_FLOAT64")
    int_type=Node::FLOAT64;
  else if(my_type=="NX_INT8")
    int_type=Node::INT8;
  else if(my_type=="NX_INT16")
    int_type=Node::INT16;
  else if(my_type=="NX_INT32")
    int_type=Node::INT32;
  else if(my_type=="NX_UINT8")
    int_type=Node::UINT8;
  else if(my_type=="NX_UINT16")
    int_type=Node::UINT16;
  else if(my_type=="NX_UINT32")
    int_type=Node::UINT32;
  else
    return Attr(name,NULL,0,NX_CHAR);
  
  int rank=1;
  int dims[rank];
  if(int_type==Node::CHAR)
    dims[0]=my_val.size();
  else
    dims[0]=1;

  void *data;
  NXmalloc(&data,rank,dims,int_type);
  try{
    void_ptr_from_string(data,my_val,rank,dims,int_type);
  }catch(std::invalid_argument &e){
    NXfree(&data);
    throw e;
  }
  Attr attr(name,data,dims[0],int_type);
  NXfree(&data);
  return attr;
}

static void my_startDocument(void *user_data){
  //cout << "startDocument" << endl; // REMOVE
}

static void my_endDocument(void *user_data){
  //cout << "endDocument" << endl; // REMOVE
}

static void my_characters(void *user_data, const xmlChar *ch, int len){
#ifdef DEBUG3_XML_PARSER
  std::cout << "characters" << std::endl;
#endif
  // convert to a string
  string str=xmlChar_to_str(ch,len);
  // if it is empty just return
  if(str.size()<=0) return;

  // add the characters with a space between it and what was there
  if(((UserData *)user_data)->char_data.size()>0)
    ((UserData *)user_data)->char_data+=" ";
  ((UserData *)user_data)->char_data+=str;
}

void my_startElement(void *user_data, const xmlChar *name,
                                                       const xmlChar ** attrs){
#ifdef DEBUG1_XML_PARSER
  std::cout << "startElement(" << name << ")" << std::endl;
#endif
  static const string LEFT  = "[";
  static const string RIGHT = "]";

  // convert the name to a string
  string str_name=xmlChar_to_str(name,-1);
  // create a label for the element when writing out exceptions
  string except_label="<"+str_name+">:";
  // convert the attributes to a vector<string>
  StrVector str_attrs=xmlattr_to_strvec(attrs);

  // check if it is a link
  bool is_link=(str_name==LINK);

  // check for "name", "type", "source", "mime_type", "location",
  // "target" attributes
  string source;
  string mime_type;
  string location;
  string type;
  bool update_dims=false;
  vector<Attr> node_attrs;
  for( StrVector::iterator it=str_attrs.begin() ; it!=str_attrs.end() ; it+=2){
    if( (*it==SOURCE) || (*it==MIME_TYPE) || (*it==LOCATION) || (*it==TYPE)
                               || ((*it==TARGET) && (is_link)) || (*it==NAME)){
      if(*it==SOURCE){
        source=*(it+1);
      }else if(*it==MIME_TYPE){
        mime_type=*(it+1);
      }else if(*it==LOCATION){
        location=*(it+1);
      }else if(*it==NAME){
        type=str_name;
        str_name=*(it+1);
      }else if(*it==TYPE){
        type=*(it+1);
        if(string_util::starts_with(type,"NX_CHAR")){
          type="NX_CHAR";
        }else if(type.substr(type.size()-1,type.size())==RIGHT){
          int start=type.find(LEFT);
          string dim=type.substr(start,type.size());
          if(dim.size()>0){
            ((UserData *)user_data)->dims=string_util::str_to_intVec(dim);
            update_dims=true;
          }else{
            ((UserData *)user_data)->dims.clear();
            ((UserData *)user_data)->dims.push_back(1);
          }
          type=type.erase(start,type.size());
        }else{
          ((UserData *)user_data)->dims.clear();
          ((UserData *)user_data)->dims.push_back(1);
        }
      }else if((is_link) && (*it==TARGET)){ // working with a link
        StrVector str_vec;
        NodeVector node_vec=((UserData *)user_data)->nodes;
        for( NodeVector::const_iterator node_it=node_vec.begin() ; node_it!=node_vec.end() ; node_it++ ){
          if(node_it->name()!=NXROOT)
            str_vec.push_back(node_it->name());
        }
        ((UserData *)user_data)->loc_to_source.push_back(str_vec);
        ((UserData *)user_data)->loc_to_source.push_back(string_util::string_to_path(*(it+1)));
        type=LINK;
      }
      str_attrs.erase(it,it+2);
      it-=2;
    }else{ // everything else is an attribute
      try{
        node_attrs.push_back(make_attr(*it,*(it+1)));
      }catch(std::invalid_argument &e){
        print_error(((UserData *)user_data),INVALID_ARGUMENT+except_label+e.what());
      }
    }
  }
  ((UserData *)user_data)->is_link=is_link;

  // if type is not defined (and it is not root) it is a character array
  if( (type.size()<=0) && (str_name!=NXROOT) )
    type="NX_CHAR";

  // map everything using the macro replacement
  map_string(((UserData *)user_data)->map,source);
  map_string(((UserData *)user_data)->map,mime_type);
  map_string(((UserData *)user_data)->map,location);

  // set the mime_type
  if(mime_type.size()<=0){
    if(((UserData *)user_data)->mime_types.size()<=0)
      mime_type=DEFAULT_MIME_TYPE;
    else
      mime_type=*(((UserData *)user_data)->mime_types.rbegin());
  }
  ((UserData *)user_data)->mime_types.push_back(mime_type);

  // confirm that maximum node depth is not exceded
  if(((UserData *)user_data)->mime_types.size()>MAX_NODE_DEPTH)
    throw runtime_error("Exceded maximum node depth");

  // create a new retriever if necessary
  RetrieverPtr retriever(NULL);
  if(source.size()>0 && mime_type.size()>0){
    try{
      retriever=Retriever::getInstance(mime_type,source);
    }catch(runtime_error &e){
      print_error(((UserData *)user_data),RUNTIME_ERROR+except_label+e.what());
    }catch(exception &e){
      print_error(((UserData *)user_data),EXCEPTION+except_label+e.what());
    }
  }else if(((UserData *)user_data)->retrievers.size()>0){
    retriever=*(((UserData *)user_data)->retrievers.rbegin());
  }
  ((UserData *)user_data)->retrievers.push_back(retriever);

  // create a new node
  bool node_from_retriever=false;
  Node node(str_name,type);  // default
  tree<Node> tree;
  if(location.size()>0 && retriever){ // if there is a location and a retriever
    try{
      retriever->getData(location,tree);
      tree.begin()->set_name(str_name);
      if( (tree.begin()->is_data()) && update_dims )
        tree.begin()->update_dims(((UserData *)user_data)->dims);
      node=*(tree.begin());
      node_from_retriever=true;
    }catch(invalid_argument &e){
      print_error(((UserData *)user_data),INVALID_ARGUMENT+except_label+e.what());
    }catch(runtime_error &e){
      print_error(((UserData *)user_data),RUNTIME_ERROR+except_label+e.what());
    }catch(exception &e){
      print_error(((UserData *)user_data),EXCEPTION+except_label+e.what());
    }
  }

  // mutate the attributes if necessary
  if(node_attrs.size()>0)
    if(node_from_retriever)
      tree.begin()->update_attrs(node_attrs);
    else
      node.update_attrs(node_attrs);

  // add the node to the end of the vector
  ((UserData *)user_data)->nodes.push_back(node);

  // check that the node is a group or data
  if(!is_link){
    if(node_from_retriever){
      NXhandle *handle=((UserData *)user_data)->handle;
      // write the data to the file
      try{
        nexus_util::make_data(handle,tree);
        nexus_util::open(handle,node);
      }catch(runtime_error &e){
        print_error(((UserData *)user_data),RUNTIME_ERROR+except_label+e.what());
      }catch(exception &e){
        print_error(((UserData *)user_data),EXCEPTION+except_label+e.what());
      }
    }else if( !node.is_data()){  // create group and open it
      NXhandle *handle=((UserData *)user_data)->handle;
      nexus_util::open(handle,node);
    }
  }
}

void my_endElement(void *user_data, const xmlChar *name){
#ifdef DEBUG1_XML_PARSER
  std::cout << "endElement(" << name << ")" << std::endl;
#endif

  // an alias for whether the current node is a link
  bool is_link=((UserData *)user_data)->is_link;

  // create a label for the element when writing out exceptions
  string except_label="</"+xmlChar_to_str(name,-1)+">";

  // get an alias to the node, this uses the copy constructor
  Node node=*(((UserData *)user_data)->nodes.rbegin());
  // get an alias to the handle
  NXhandle *handle=((UserData *)user_data)->handle;
  
  // deal with character data if necessary
  if(((UserData *)user_data)->char_data.size()>0){
    // update the node with the character value
    try{
      try{
        update_node_from_string(node,((UserData *)user_data)->char_data,
                               ((UserData *)user_data)->dims, node.int_type());
      }catch(runtime_error &e){
        print_error(((UserData *)user_data),RUNTIME_ERROR+": "+except_label+" group cannot contain data");
      }
    }catch(invalid_argument &e){
      print_error(((UserData *)user_data),
                                       INVALID_ARGUMENT+except_label+e.what());
      return;
    }

    // clear out the dimensions value
    ((UserData *)user_data)->dims.clear();
    // clear out the character value
    ((UserData *)user_data)->char_data="";

    // write the data to the file
    if(!is_link){
      nexus_util::open(handle,node);
      try{
        nexus_util::make_data(handle,node);
      }catch(runtime_error &e){
        print_error(((UserData *)user_data),RUNTIME_ERROR+except_label+e.what());
      }catch(exception &e){
        print_error(((UserData *)user_data),EXCEPTION+except_label+e.what());
      }
    }
  }
  
  // close the node
  if(is_link)
    ((UserData *)user_data)->is_link=false;
  else
    nexus_util::close(handle,node);

  // pop the node off of the end of the vector
  ((UserData *)user_data)->nodes.pop_back();
  // pop the mime_type off of the end of the vector
  ((UserData *)user_data)->mime_types.pop_back();
  // pop the retriever off of the end of the vector
  ((UserData *)user_data)->retrievers.pop_back();
}

static xmlEntityPtr my_getEntity(void *user_data, const xmlChar *name){
  return xmlGetPredefinedEntity(name);
}

static void my_error(void *user_data, const char* msg, ...){
  static const string SAX_ERROR="SAX_ERROR";

  // get the rest of the arguments
  va_list args;
  va_start(args,msg);

  // get the position of the error
  int line=getLineNumber(context);
  int col =getColumnNumber(context);

  // print out the result
  char str[70];
  int num_out=vsprintf(str,msg,args);
  cerr << SAX_ERROR << " [L" << line << " C" << col << "]: "<< str;

  // clean up args
  va_end(args);

  // set the status to failure
  ((UserData *)user_data)->status=-1;
}

static void my_fatalError(void *user_data, const char* msg, ...){
  static const string FATAL_SAX_ERROR="FATAL_SAX_ERROR";

  // get the rest of the arguments
  va_list args;
  va_start(args,msg);

  // get the position of the error
  int line=getLineNumber(context);
  int col =getColumnNumber(context);

  // print out the result
  char str[70];
  int num_out=vsprintf(str,msg,args);
  cerr << FATAL_SAX_ERROR << " [L" << line << " C" << col << "]: "<< str;

  // clean up args
  va_end(args);

  // set the status to failure
  ((UserData *)user_data)->status=-1;
}

static xmlSAXHandler my_handler = {
  NULL, // internalSubsetSAXFunc internalSubset;
  NULL, // isStandaloneSAXFunc isStandalone;
  NULL, // hasInternalSubsetSAXFunc hasInternalSubset;
  NULL, // hasExternalSubsetSAXFunc hasExternalSubset;
  NULL, // resolveEntitySAXFunc resolveEntity;
  my_getEntity, // getEntitySAXFunc getEntity;
  NULL, // entityDeclSAXFunc entityDecl;
  NULL, // notationDeclSAXFunc notationDecl;
  NULL, // attributeDeclSAXFunc attributeDecl;
  NULL, // elementDeclSAXFunc elementDecl;
  NULL, // unparsedEntityDeclSAXFunc unparsedEntityDecl;
  NULL, // setDocumentLocatorSAXFunc setDocumentLocator;
  my_startDocument, // startDocumentSAXFunc startDocument;
  my_endDocument, // endDocumentSAXFunc endDocument;
  my_startElement, // startElementSAXFunc startElement;
  my_endElement, // endElementSAXFunc endElement;
  NULL, // referenceSAXFunc reference;
  my_characters, // charactersSAXFunc characters;
  NULL, // ignorableWhitespaceSAXFunc ignorableWhitespace;
  NULL, // processingInstructionSAXFunc processingInstruction;
  NULL, // commentSAXFunc comment;
  NULL, // warningSAXFunc warning;
  my_error, // errorSAXFunc error;
  my_fatalError, // fatalErrorSAXFunc fatalError;
};

static bool resolve_links(UserData *user_data){
  // check that this function will do anything
  if(user_data->loc_to_source.size()<=0) return false;

  // convenience for less typing
  typedef vector<StrVector>::const_iterator vec_iter;

  // loop over links in map
  for( vec_iter it=user_data->loc_to_source.begin() ; it!=user_data->loc_to_source.end() ; it+=2 ){
    nexus_util::make_link(user_data->handle,*it,*(it+1));
  }


  // return that everything went well
  return false;
}

extern bool xml_parser::parse_xml_file(const std::map<string,string> &map,
                                       const string &filename,
                                       NXhandle *handle){
#ifdef DEBUG3_XML_PARSER
  std::cout << "xml_parser::parse_xml_file" << std::endl;
#endif
  // set up the user data for use in the parser
  UserData user_data;
  user_data.handle=handle;
  user_data.status=0;
  user_data.is_link=false;
  user_data.map=map;
  user_data.mime_types.reserve(MAX_NODE_DEPTH);
  user_data.dims.reserve(25);
  user_data.retrievers.reserve(MAX_NODE_DEPTH);

  // parse the translation file (context needed to get positions in the file)
  context=xmlCreateFileParserCtxt(filename.c_str());
  context->sax=&my_handler;
  context->userData=&user_data;
  int result=xmlParseDocument(context);

  // return if there was an error
  if(user_data.status)
    return user_data.status; // return the error
  else if(result<0)
    return true; // return generic error

  // work on links
  try{
    resolve_links(&user_data);
  }catch(runtime_error &e){ // deal with problems
    cerr << RUNTIME_ERROR << ": " << e.what() << endl;
    return true;
  }
  
  // return that everything went well
  return false;
}
