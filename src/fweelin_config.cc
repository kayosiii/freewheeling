// A deer crossed my path today
//
// It said
//
// Why all this pressure 
// to change the world??
//
// Accept the world as it is.
// That is peace.
// That is healing.
//
// It is not idleness.
// It is precise action,
// Energy conservation.

#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include "fweelin_config.h"
#include "fweelin_midiio.h"
#include "fweelin_sdlio.h"
#include "fweelin_core.h"

// *********** CONFIG

#ifdef __MACOSX__
#import <sys/param.h> /* for MAXPATHLEN */

char FWEELIN_datadir[MAXPATHLEN];
char *FWEELIN_DATADIR = FWEELIN_datadir;
#endif

const char CfgMathOperation::operators[] = {'/', '*', '+', '-'};
const int CfgMathOperation::numops = 4;

const int FloConfig::NUM_PREALLOCATED_AUDIO_BLOCKS = 40;
const float FloConfig::AUDIO_MEMORY_LEN = 10.0;

void UserVariable::Print(char *str, int maxlen) {
  if (name != 0 && str == 0)
    printf("'%s'[",name);

  switch (type) {
  case T_char : 
    if (str != 0)
      snprintf(str,maxlen,"%d",(int) *this);
    else
      printf("%d",(int) *this);
    break;
  case T_int : 
    if (str != 0)
      snprintf(str,maxlen,"%d",(int) *this);
    else
      printf("%d",(int) *this);
    break;
  case T_long : 
    if (str != 0)
      snprintf(str,maxlen,"%ld",(long) *this);
    else
      printf("%ld",(long) *this);
    break;
  case T_float : 
    if (str != 0)
      snprintf(str,maxlen,"%.2f",(float) *this);
    else
      printf("%.2f",(float) *this);
    break;
  case T_range :
    {
      Range r = *this;
      if (str != 0)
	snprintf(str,maxlen,"%d>%d",r.lo,r.hi);
      else
	printf("%d>%d",r.lo,r.hi);
    }
    break;
  case T_variable :
    if (str != 0)
      snprintf(str,maxlen,"variable");
    else
      printf("variable");
    break;
  case T_variableref :
    if (str != 0)
      snprintf(str,maxlen,"variable reference");
    else
      printf("variable reference");
    break;
  case T_invalid : 
    if (str != 0)
      snprintf(str,maxlen,"invalid");
    else
      printf("invalid");
    break;
  }

  if (name != 0 && str == 0)
    printf("]");
};

void CfgToken::Print() {
  switch (cvt) {
  case T_CFG_None :
    printf("[none]\n");
    break;
  case T_CFG_Static :
    val.Print();
    break;
  case T_CFG_EventParameter :
    printf("'%s'",evparam.name); 
    break;
  case T_CFG_UserVariable :
    var->Print();
    break;
  }
};
 
// Evaluate the current value of this token to dst
// Using event ev as a reference for event parameter
// If overwritetype is nonzero, sets dst to be of the appropriate data type
// Otherwise, converts to existing type of dst 
void CfgToken::Evaluate(UserVariable *dst, Event *ev, char overwritetype) {
  switch (cvt) {
  case T_CFG_None :
    break;
  case T_CFG_Static :
    if (overwritetype)
      dst->type = val.type;
    dst->SetFrom(val);
    break;
  case T_CFG_EventParameter :
    if (ev != 0) {
      UserVariable tmp;
      tmp.type = evparam.dtype;
      char *evofs = (char *)ev + evparam.ofs;
      
      switch (evparam.dtype) {
      case T_char :
	memcpy(tmp.value,evofs,sizeof(char));
	break;
	
      case T_int :
	memcpy(tmp.value,evofs,sizeof(int));
	break;
	
      case T_long :
	memcpy(tmp.value,evofs,sizeof(long));
	break;
	
      case T_float :
	memcpy(tmp.value,evofs,sizeof(float));
	break;
	
      default :
	printf(" CfgToken: Can't evaluate invalid data type\n");
      }
      
      if (overwritetype)
	dst->type = tmp.type;
      dst->SetFrom(tmp);
    }
    break;
  case T_CFG_UserVariable :
    if (overwritetype)
      dst->type = var->type;
    dst->SetFrom(*var);
    break;
  }
};

char ParsedExpression::IsStatic() {
  if (start.cvt != T_CFG_Static)
    return 0;
  
  // Check math ops for nonstatics
  CfgMathOperation *cur = ops;
  while (cur != 0) {
    if (cur->operand.cvt != T_CFG_Static) 
      return 0;
    cur = cur->next;
  }

  return 1;
};

void ParsedExpression::Print() {
  // Print starting token
  start.Print();
  
  CfgMathOperation *cop = ops;
  while (cop != 0) {
    printf("%c",cop->otype);
    cop->operand.Print();
    cop = cop->next;
  }
};

// Evaluate this expression
UserVariable ParsedExpression::Evaluate(Event *input) {
  // Starting token..
  UserVariable cur;
  start.Evaluate(&cur,input,1); // Setup cur with evaluation of start token

  // Move through the math..
  CfgMathOperation *cop = ops;
  while (cop != 0) {
    // Evaluate the operand
    UserVariable tmp;
    cop->operand.Evaluate(&tmp,input,1);

    switch (cop->otype) {
    case '/' :
      cur /= tmp;
      break;
    case '*' :
      cur *= tmp;
      break;
    case '+' :
      cur += tmp;
      break;
    case '-' :
      cur -= tmp;
      break;
    default :
      printf("Evaluate Expression: Invalid math operand\n");
    }
    
    cop = cop->next;
  }

  return cur;
};

EventBinding::~EventBinding() {
  // Erase prototype event
  if (boundproto != 0)
    boundproto->RTDelete();

  {
    // Erase dynamic token conditions
    DynamicToken *cur = tokenconds;
    while (cur != 0) {
      DynamicToken *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }
  
  {
    // Erase dynamic parameter sets
    DynamicToken *cur = paramsets;
    while (cur != 0) {
      DynamicToken *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }
};

InputMatrix::InputMatrix(Fweelin *app) : vars(0), app(app) {
  // Setup input bindings array
  input_bind = new EventBinding **[T_EV_Last_Bindable];
  for (int i = 0; i < T_EV_Last_Bindable; i++)
    input_bind[i] = 0;
};

void InputMatrix::Start() {
  // Setup IM to listen!- this can not be done earlier because, during
  // configuration, EMG is not active

  // Listen for input events
  if (app->getEMG() == 0) {
    printf("INIT: Error: Event Manager not yet active!\n");
    exit(1);
  }

  app->getEMG()->ListenEvent(this,0,T_EV_SetVariable);
  app->getEMG()->ListenEvent(this,0,T_EV_ToggleVariable);
  app->getEMG()->ListenEvent(this,0,T_EV_ShowDebugInfo);
  app->getEMG()->ListenEvent(this,0,T_EV_AdjustMidiTranspose);

  // Input events
  int evnum = (int) EventType(T_EV_Last_Bindable);
  for (int i = 0; i < evnum; i++)
    if (i == (int) EventType(T_EV_GoSub))
      // Listen for GoSub, but allow us to call ourselves because
      // that is what gosub does!
      app->getEMG()->ListenEvent(this,0,T_EV_GoSub);
    else
      // Listen for input events, blocking receive of calls from ourself to
      // prevent infinite loops on event echo
      app->getEMG()->ListenEvent(this,0,(EventType) i,1);
};

InputMatrix::~InputMatrix() {
  EventBinding *cur, *tmp;

  app->getEMG()->UnlistenEvent(this,0,T_EV_SetVariable);
  app->getEMG()->UnlistenEvent(this,0,T_EV_ToggleVariable);
  app->getEMG()->UnlistenEvent(this,0,T_EV_ShowDebugInfo);
  app->getEMG()->UnlistenEvent(this,0,T_EV_AdjustMidiTranspose);

  // Input events
  int evnum = (int) EventType(T_EV_Last_Bindable);
  for (int i = 0; i < evnum; i++) {
    // Stop listening
    app->getEMG()->UnlistenEvent(this,0,(EventType) i);
    
    EventBinding **cur_hash = input_bind[i];
    if (cur_hash != 0) {
      // & Free data structures
      int pidx = Event::GetParamIdxByType((EventType) i);
      if (pidx == -1)
	delete cur_hash; // No hash array, just one
      else {
	Event *tmpev = Event::GetEventByType((EventType) i,1);
	int hashsz = tmpev->GetParam(pidx).max_index+1;
	tmpev->RTDelete();

	for (int j = 0; j < hashsz; j++) {
	  cur = cur_hash[j];
	  while (cur != 0) {
	    tmp = cur->next;
	    delete cur;
	    cur = tmp;
	  }
	};

	delete[] cur_hash;
      }
    }
  }

  delete[] input_bind;

  {
    UserVariable *cur = vars;
    while (cur != 0) {
      UserVariable *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }
};

// Removes leading and trailing spaces from string str
// Modifies the end of string str and returns a pointer to the new 
// beginning after spaces
char *InputMatrix::RemoveSpaces (char *str) {
  char *curptr = str;
  while (*curptr == ' ')
    curptr++;
  int n = strlen(str)-1;
  while (n > 0 && str[n] == ' ') {
    str[n] = '\0';
    n--;
  }

  return curptr;
};

// Adds one key to the given list based on the keysym name
// Returns the new first pointer
SDLKeyList *InputMatrix::AddOneKey (SDLKeyList *first, char *str) {
  // Remove leading and trailing spaces
  str = RemoveSpaces(str);

  SDLKey kh_sym = SDLIO::GetSDLKey(str);
  if (kh_sym != SDLK_UNKNOWN) {
    printf("'%s' + ",str);

    // Link it in!
    if (first == 0)
      first = new SDLKeyList(kh_sym);
    else {
      SDLKeyList *cur = first;
      while (cur->next != 0)
	cur = cur->next;
      cur->next = new SDLKeyList(kh_sym);
    }
  }
  else
    printf("** UNKNOWN KEY '%s' ** + ",str);

  return first;
};

// Extracts named keys from the given string and returns a list
// of the keysyms (named keys are separated by ,)
SDLKeyList *InputMatrix::ExtractKeys (char *str) {
  char buf[255]; // Copy buf
  char *delim = ","; 

  // Go through list of keys specified:
  char *cur = strpbrk(str,delim);
  int firstopidx;
  if (cur != 0) 
    firstopidx = (long)cur-(long)str;
  else
    firstopidx = -1;
  
  // First key
  if (firstopidx == -1) {
    strncpy(buf,str,255);
    buf[254] = '\0';
  }
  else {
    long len = MIN(firstopidx,254);
    memcpy(buf,str,len);
    buf[len] = '\0';
  }

  // Parse it
  SDLKeyList *first = 0;
  first = AddOneKey(first,buf);

  while (cur != 0) {
    cur++;
    char *next = strpbrk(cur,delim);

    // Copy the key name
    long len;
    if (next != 0)
      len = (long)next-(long)cur;
    else
      len = strlen(cur);
    if (len >= 255)
      len = 254;
    memcpy(buf,cur,len);
    buf[len] = '\0';

    // And parse it
    first = AddOneKey(first,buf);

    cur = next;
  }

  return first;
};

void InputMatrix::SetVariable (UserVariable *var, char *value) {
  // First, parse the value based on the variable type
  
  switch (var->type) {
  case T_char :
    *var = (char) atoi(value);
    break;
  case T_int :
    *var = (int) atoi(value);
    break;
  case T_long :
    *var = (long) atol(value);
    break;
  case T_float :
    *var = (float) atof(value);
    break;
  case T_range :
    {
      char tmp[255];
      strncpy(tmp,value,255);
      tmp[254] = '\0';
      char *delim = strchr(tmp,'>');
      if (delim != 0) {
	*delim = '\0';
	delim++;
      }

      int lo = atoi(tmp),
	hi = (delim != 0 ? atoi(delim) : 0);     
      Range r(lo,hi);
      *var = r;
    }
    break;
  case T_variable : 
  case T_variableref : 
  case T_invalid : 
    printf("SetVariable: Invalid variable type!\n");
    break;
  }
};

void InputMatrix::CreateVariable (xmlNode *declare) {
  UserVariable *nw = new UserVariable();

  // Name
  xmlChar *name = xmlGetProp(declare, (const xmlChar *)"var");
  if (name == 0) {
    printf(FWEELIN_ERROR_COLOR_ON
	   "*** INIT: WARNING: Variable name not specified when declaring.\n"
	   FWEELIN_ERROR_COLOR_OFF);
    delete nw;
    return;
  }
  nw->name = new char[xmlStrlen(name)+1];
  strcpy(nw->name,(char *)name);
  xmlFree(name);

  // Type
  xmlChar *type = xmlGetProp(declare, (const xmlChar *)"type");
  if (type == 0) {
    printf(FWEELIN_ERROR_COLOR_ON
	   "*** INIT: WARNING: Variable type not specified when declaring.\n"
	   FWEELIN_ERROR_COLOR_OFF);
    delete nw;
    return;
  }
  nw->type = GetCoreDataType((char *) type);
  if (nw->type == T_invalid) {
    printf(FWEELIN_ERROR_COLOR_ON
	   "*** INIT: WARNING: Invalid variable type.\n"
	   FWEELIN_ERROR_COLOR_OFF);
    delete nw;
    return;
  }

  // Value
  xmlChar *value = xmlGetProp(declare, (const xmlChar *)"init");
  if (value == 0)
    memset(nw->value,0,CFG_VAR_SIZE);
  else
    SetVariable(nw,(char *) value);

  // Insert into variable list
  nw->next = vars;
  vars = nw;

  printf(" declare: variable '%s' type '%s' value '%s'\n", nw->name, type, 
	 value);
  xmlFree(type);
  if (value != 0)
    xmlFree(value);
};

// Scans in the given binding for settings for output event parameters
// and sets us up to handle those
void InputMatrix::CreateParameterSets (EventBinding *bind, xmlNode *binding, 
				       Event *input, int contnum) {
  const static char *delim = " and ";

  const static char *str_base = "parameters";
  int str_len = strlen(str_base)+4;
  char basebuf[str_len];
  if (contnum == 0)
    sprintf(basebuf,str_base);
  else 
    snprintf(basebuf,str_len,"%s%d",str_base,contnum);

  xmlChar *paramstr = xmlGetProp(binding, (const xmlChar *)basebuf);
  if (paramstr != 0) {
    // Config specifies param sets
    
    // Separate into each param set
    char *buf = new char[xmlStrlen(paramstr)+1],
      *curp = (char *)paramstr, 
      *nextp;

    do {
      nextp = strstr(curp,delim);

      int len = (nextp == 0 ? strlen(curp) : (int) (nextp-curp));
      strncpy(buf,curp,len);
      buf[len] = '\0';

      // Now buf contains one parameter- parse it
      
      // Get LValue
      char *sep = strchr(buf,'=');
      if (sep != 0) {
	*sep = '\0';
	
	// Remove leading and trailing spaces
	char *lv = RemoveSpaces(buf);

	// Find extra tokens not properly formatted
	if (strchr(sep+1,'=') != 0) {
	  char *rv = RemoveSpaces(sep+1);
	  printf(FWEELIN_ERROR_COLOR_ON
		 "*** INIT: WARNING: Missing 'and' in parameters token: "
                 "'%s=%s'\n"
		 FWEELIN_ERROR_COLOR_OFF,
		 lv,rv);
	}

	// Get RValue
	char *rv = RemoveSpaces(sep+1);
	
	// Check LValue against output parameters
	char found = 0;
	for (int i = 0; !found && i < bind->boundproto->GetNumParams(); i++) {
	  EventParameter param = bind->boundproto->GetParam(i);
	  
	  if (!strcmp(lv,param.name)) {
	    // Config specifies a param set for this parameter
	    found = 1;

	    // Parse RValue expression that specifies set.
	    char enable_keynames = (bind->boundproto->GetType() == 
				    T_EV_Input_Key &&
				    !strcmp(param.name,"key"));
	    ParsedExpression *exp = 
	      ParseExpression(rv, input, enable_keynames);

	    // OK, check if expression is static
	    if (exp->IsStatic()) {
	      // Yup, so evaluate now
	      UserVariable val = exp->Evaluate(input);
	      
	      // Store directly in output prototype event
	      char *nwofs = 
		(char *)bind->boundproto + param.ofs; // Data location
	      StoreParameter(nwofs,param.dtype,&val);
	      printf("         -set '%s' = ",param.name);
	      val.Print();
	      printf("\n");

	      delete exp;
	    }
	    else {
	      // Dynamic expression

	      DynamicToken *nwset = new DynamicToken();
	      CfgToken token;
	      token.cvt = T_CFG_EventParameter;
	      token.evparam = param;
	      nwset->token = token;
	      nwset->exp = exp;
	      nwset->next = bind->paramsets;
	      bind->paramsets = nwset;
	      
	      printf("         -set '%s' = ",param.name);
	      exp->Print();
	      printf("\n");
	    }
	  }
	}

	// Debugging for invalid paramset
	if (!found) {
	  printf(FWEELIN_ERROR_COLOR_ON
		 "*** INIT: WARNING: Invalid parameters token: '%s=%s'\n"
		 FWEELIN_ERROR_COLOR_OFF,
		 lv,rv);
	}
      }

      // Next condition
      curp = (nextp == 0 ? 0 : nextp+strlen(delim));
    } while (curp != 0);
  
    delete[] buf;
    xmlFree(paramstr);
  }
};

// Scans in the given binding for conditions on input event parameters 
// or user variables, and sets us up to handle those
// Returns the hash index for this binding, based on an indexed parameter,
// or 0 if this binding is not indexed
int InputMatrix::CreateConditions (EventBinding *bind, xmlNode *binding, 
				   Event *input, int paramidx) {
  const static char *delim = " and ";

  // Return index
  int ret_index = 0;

  xmlChar *cond = xmlGetProp(binding, (const xmlChar *)"conditions");
  if (cond != 0) {
    // Config specifies conditions
    
    // Separate into each condition
    char *buf = new char[xmlStrlen(cond)+1],
      *curc = (char *)cond, 
      *nextc;

    do {
      nextc = strstr(curc,delim);

      int len = (nextc == 0 ? strlen(curc) : (int) (nextc-curc));
      strncpy(buf,curc,len);
      buf[len] = '\0';

      // Now buf contains one condition- parse it
      
      // Get LValue
      char *sep = strchr(buf,'=');
      if (sep != 0) {
	*sep = '\0';

	// Remove leading and trailing spaces
	char *lv = RemoveSpaces(buf);

	// Find extra tokens not properly formatted
	if (strchr(sep+1,'=') != 0) {
	  char *rv = RemoveSpaces(sep+1);
	  printf(FWEELIN_ERROR_COLOR_ON
		 "*** INIT: WARNING: Missing 'and' in conditions token: "
                 "'%s=%s'\n"
		 FWEELIN_ERROR_COLOR_OFF,
		 lv,rv);
	}

	// Get RValue
	char *rv = RemoveSpaces(sep+1);
	
	// Check LValue against input parameters
	char found = 0;
	if (input != 0) {
	  for (int i = 0; !found && i < input->GetNumParams(); i++) {
	    EventParameter param = input->GetParam(i);
	    
	    if (!strcmp(lv,param.name)) {
	      // Config specifies a condition for this parameter
	      found = 1;

	      // Parse RValue expression that specifies condition.
	      char enable_keynames = (input->GetType() == T_EV_Input_Key &&
				      !strcmp(param.name,"key"));
	      ParsedExpression *exp = 
		ParseExpression(rv, input, enable_keynames);

	      // Is this parameter indexed?
	      if (i == paramidx) {
		// OK, is the RValue static (ie can we store the binding 
                // in a hash or do we need to compute during runtime)?
		if (exp->IsStatic()) {
		  // Yup, so evaluate now
		  UserVariable val = exp->Evaluate(input);

		  // Return hash value
		  ret_index = (int) val % param.max_index;
		} else {
		  // Indexed parameter, but value is not static-
		  // Use wildcard slot in hash
		  ret_index = param.max_index;
		}
	      }
	
	      // Create DynamicToken to hold condition
	      DynamicToken *nwcond = new DynamicToken();
	      CfgToken token;
	      token.cvt = T_CFG_EventParameter;
	      token.evparam = param;
	      nwcond->token = token;
	      nwcond->exp = exp;
	      nwcond->next = bind->tokenconds;
	      bind->tokenconds = nwcond;
	
	      printf("         -condition '%s' == ",param.name);
	      exp->Print();
	      if (enable_keynames) 
		printf(" [%s]",rv);
	      printf("\n");
	    }
	  }
	}

	// Check LValue against user variables
	UserVariable *cur = vars;
	while (!found && cur != 0) {
	  if (cur->name != 0) { 
	    if (!strcmp(lv,cur->name)) {
	      // Config specifies a condition for this variable
	      found = 1;

	      // Parse RValue expression that specifies condition.
	      ParsedExpression *exp = ParseExpression(rv, input);
	
	      // Create DynamicToken to hold condition
	      DynamicToken *nwcond = new DynamicToken();
	      CfgToken token;
	      token.cvt = T_CFG_UserVariable;
	      token.var = cur;
	      nwcond->token = token;
	      nwcond->exp = exp;
	      nwcond->next = bind->tokenconds;
	      bind->tokenconds = nwcond;
	      
	      printf("         -condition '%s' == ",cur->name);
	      exp->Print();
	      printf("\n");
	    }
	  }
	  cur = cur->next;
	}

	// Debugging for invalid condition
	if (!found) {
	  printf(FWEELIN_ERROR_COLOR_ON
		 "*** INIT: WARNING: Invalid conditions token: '%s=%s'\n"
		 FWEELIN_ERROR_COLOR_OFF,
		 lv,rv);
	}
      }

      // Next condition
      curc = (nextc == 0 ? 0 : nextc+strlen(delim));
    } while (curc != 0);

    delete[] buf;
    xmlFree(cond);
  }

  // printf("  (hash %d)\n",ret_index);
  return ret_index;
}

void InputMatrix::CreateBinding (xmlNode *binding) {
  EventBinding *nw = 0, *prev = 0;

  // Input event
  xmlChar *instr = xmlGetProp(binding, (const xmlChar *)"input");
  if (instr == 0) {
    printf(" [Invalid binding: No input event!]\n");
  } else {
    printf(" binding: input '%s'", (char *) instr);

    // Go from the named event to an Event* prototype
    // Wait for an instance if none available
    Event *inproto = Event::GetEventByName((char *)instr,1);
    if (inproto == 0)
      printf(" [Invalid event!]\n");
    else {
      int typ = inproto->GetType();
      if (typ >= T_EV_Last_Bindable)
	printf(" [This event type can't be an input!]\n");
      else {
	int pidx = Event::GetParamIdxByType((EventType) typ);
	if (input_bind[typ] == 0) {
	  // No bindings yet for this type- set it up
	  if (pidx == -1) {
	    // No index for this type
	    input_bind[typ] = new EventBinding *;
	    input_bind[typ][0] = 0;
	  } else {
	    // Create hashtable for this type
	    // Store one extra element for wildcard case
	    int hashsz = inproto->GetParam(pidx).max_index+1;
	    printf("\nCONFIG: Create '%s' parameter hashtable[%d] for input "
		   "event '%s'\n",inproto->GetParam(pidx).name,
		   hashsz,Event::GetEventName((EventType) typ));
	    input_bind[typ] = new EventBinding *[hashsz];
	    for (int i = 0; i < hashsz; i++) 
	      input_bind[typ][i] = 0;
	  }
	}

	// OK, create a binding
	nw = new EventBinding();
    
	// Echo?
	xmlChar *echo = xmlGetProp(binding, (const xmlChar *)"echo");
	if (echo != 0) {
	  nw->echo = atoi((char *)echo);
	  if (nw->echo)
	    printf(" (echo)");
	  xmlFree(echo);
	}

	// Conditions
	printf("\n");
	int store_idx = CreateConditions(nw,binding,inproto,pidx);

	// Output event(s)
	int contnum = 0;
	char go = 1, first = 1;
	do { 
	  const static char *str_base = "output";
	  int str_len = strlen(str_base)+4;
	  char buf[str_len];
	  if (contnum == 0)
	    sprintf(buf,str_base);
	  else 
	    snprintf(buf,str_len,"%s%d",str_base,contnum);

	  xmlChar *outstr = xmlGetProp(binding, (const xmlChar *)buf);
	  if (outstr == 0) {
	    // Try at least "output" and "output1"
	    if (contnum > 0)
	      go = 0;
	  } else {
	    printf("       -> output '%s'", (char *) outstr);

	    if (nw == 0) {
	      // OK, we need another binding (continued)
	      nw = new EventBinding();
	    }

	    // Go from the named event to an Event* prototype
	    // Wait for an instance if none available
	    nw->boundproto = Event::GetEventByName((char *)outstr,1);
	    if (nw->boundproto == 0)
	      printf(FWEELIN_ERROR_COLOR_ON 
		     " [*** Invalid event! ***]\n"
		     FWEELIN_ERROR_COLOR_OFF);
	    else {
	      printf("\n");
	      
	      // Create parameter settings
	      CreateParameterSets(nw,binding,inproto,contnum);
	      
	      if (prev != 0) {	  
		// So flag previous binding as 'continued'
		prev->continued = 1;
		// And link it up to this new binding
		//prev->next = nw;
	      }
	      
	      // Store binding
	      //printf("typ: %d store_idx: %d\n",typ,store_idx);
	      //printf("ib_one: %p\n",input_bind[typ]);
	      if (input_bind[typ][store_idx] == 0)
		input_bind[typ][store_idx] = nw;
	      else {
		EventBinding *cur = input_bind[typ][store_idx];
		while (cur->next != 0)
		  cur = cur->next;
		cur->next = nw;
	      }
	      
	      prev = nw;
	      nw = 0;
	      first = 0;
	    }

	    xmlFree(outstr);
	  }

	  contnum++;
	} while (go);

	if (first)
	  printf(FWEELIN_ERROR_COLOR_ON 
		 " [*** No output event specified! ***]\n"
		 FWEELIN_ERROR_COLOR_OFF);

	if (nw != 0) {
	  // Erase unstored binding
	  delete nw;
	  nw = 0;
	}
      }

      // Erase input prototype used for querying
      inproto->RTDelete();
    }

    xmlFree(instr);
  }
};

// Parses the given token (no math ops!) into dst
// Correctly identifies when variables or event parameters are referenced
// enable_keynames means that the token is first interpreted as a keyboard
// key name
void InputMatrix::ParseToken(char *str, CfgToken *dst, Event *ref,
			     char enable_keynames) {
  //printf("parse token: %s\n",str);

  // First, remove leading or trailing spaces
  // Careful, we **overwrite 'str'**
  while (*str == ' ')
    str++;
  char *tmp = strchr(str,' ');
  if (tmp != 0)
    *tmp = '\0';

  // Parse key name?
  if (enable_keynames) {
    int keysym = SDLIO::GetSDLKey((char *) str);
    if (keysym != SDLK_UNKNOWN && keysym >= SDLK_FIRST &&
	keysym < SDLK_LAST) {
      // Token references a keyboard key!
      dst->cvt = T_CFG_Static; // Interpret as static
      dst->val.type = T_int;
      dst->val = keysym;
      return;
    }
  }

  // Check if token references an event parameter
  if (ref != 0)
    for (int j = 0; j < ref->GetNumParams(); j++) {
      EventParameter evparam = ref->GetParam(j);
      if (!strcmp(str,evparam.name)) {
	// Matching parameter
	dst->cvt = T_CFG_EventParameter;
	dst->evparam = evparam;
	return;
      }
    }

  // Check if token references a user variable
  UserVariable *cur = vars;
  while (cur != 0) {
    if (!strcmp(str,cur->name)) {
      // Matching user variable
      dst->cvt = T_CFG_UserVariable;
      dst->var = cur;
      return;
    }
    cur = cur->next;
  }

  // No matches to variables or parameters, interpret as static token
  char *ascii_scalar = " 0123456789.>,";
  if (strlen(str) > 0 && strpbrk(str,ascii_scalar) != str) {
    // Wait a second, this isn't a scalar, it probably has letters!
    printf(FWEELIN_ERROR_COLOR_ON
	   "\n*** INIT: WARNING: Invalid token: %s\n"
	   FWEELIN_ERROR_COLOR_OFF,
	   str);
    dst->cvt = T_CFG_None;
    dst->var = 0;
    return;
  }

  // Figure out what type of static the token is
  dst->cvt = T_CFG_Static;
  char *rng_delim = strchr(str,'>');
  if (rng_delim != 0) {
    // Range character found
    dst->val.type = T_range;
  } else {
    char *float_delim = strchr(str,'.');
    if (float_delim != 0) {
      // Float character found 
      dst->val.type = T_float;
    }
    else {
      // Assume scalar is 'int'--
      // This affects runtime performance in that all dynamic evaluations
      // of the parent expression are computed using this type!
      dst->val.type = T_int;
    }
  }
  
  // Given the type, set the variable!
  SetVariable(&(dst->val),str);
};

// Parses a given expression string, extracting tokens
// for example: 'VAR_curnote+12' references variable VAR_curnote and
// creates 1 math operation +12
// The expression may also reference parameters in event 'ref'
// and these references will be extracted
ParsedExpression *InputMatrix::ParseExpression(char *str, Event *ref,
					       char enable_keynames) {
  char opstr[CfgMathOperation::numops+1];
  char buf[255]; // Copy buf
  CfgMathOperation *first = 0,
    *last = 0;
  signed int firstopidx = -1;

  //printf("parse expression: %s\n",str);

  // Create list of operators
  int i;
  for (i = 0; i < CfgMathOperation::numops; i++)
    opstr[i] = CfgMathOperation::operators[i];
  opstr[i] = '\0';

  // Find the first occurance of an operator in string str
  char *cur = strpbrk(str,opstr);
  if (cur != 0) 
    firstopidx = (long)cur-(long)str;
  else
    firstopidx = -1;
  
  // Now, begin creating parsed expression
  ParsedExpression *exp = new ParsedExpression();
  // Parse beginning token
  if (firstopidx == -1) {
    strncpy(buf,str,255);
    buf[254] = '\0';
  }
  else {
    long len = MIN(firstopidx,254);
    memcpy(buf,str,len);
    buf[len] = '\0';
  }
  ParseToken(buf,&(exp->start),ref,enable_keynames);

  while (cur != 0) {
    char op = *cur; // Store operand
    cur++;
    char *next = strpbrk(cur,opstr);

    // Copy the operand
    long len;
    if (next != 0)
      len = (long)next-(long)cur;
    else
      len = strlen(cur);
    if (len >= 255)
      len = 254;
    memcpy(buf,cur,len);
    buf[len] = '\0';

    // Now we have operator 'op' and operand in 'buf'
    CfgMathOperation *nw = new CfgMathOperation();
    nw->otype = op;
    ParseToken(buf,&(nw->operand),ref,enable_keynames);
    if (first == 0)
      first = last = nw;
    else {
      last->next = nw;
      last = nw;
    }

    cur = next;
  }

  exp->ops = first;
  return exp;
};

// Stores in ptr the value val given that ptr is of type dtype
void InputMatrix::StoreParameter(char *ptr, CoreDataType dtype, 
				 UserVariable *val) {
  switch (dtype) {
  case T_char :
    *ptr = (char) *val;
    break;
    
  case T_int :
    *((int *) ptr) = (int) *val;
    break;
    
  case T_long :
    *((long *) ptr) = (long) *val;
    break;
    
  case T_float :
    *((float *) ptr) = (float) *val;
    break;

  case T_range :
    *((Range *) ptr) = (Range) *val;
    break;

  case T_variable :
    *((UserVariable *) ptr) = *val;
    break;

  case T_variableref :
    *((UserVariable **) ptr) = val;
    break;

  default :
    printf("Unrecognized event parameter type!\n");
    break;
  }
};

// Using the eventbinding's parametersets as a template, dynamically
// sets parameters in the output event
void InputMatrix::SetDynamicParameters(Event *input, Event *output,
				       EventBinding *bind) {
  // Go through the settings 
  DynamicToken *cur = bind->paramsets;
  while (cur != 0) {
    if (cur->token.cvt == T_CFG_EventParameter) {
      // Get ptr to output parameter
      char *outofs = (char *)output + cur->token.evparam.ofs;

      if (cur->token.evparam.dtype == T_variableref) {
	// Output event wants a reference to a variable-- not an evaluation!
	// See if the starting token is a UserVariable
	if (cur->exp->start.cvt == T_CFG_UserVariable)
	  StoreParameter(outofs,T_variableref,cur->exp->start.var);
	else {
	  printf("CONFIG: SetDynamicParameters: Event expects UserVariable but"
		 " another type is given by config!\n");
	  StoreParameter(outofs,T_variableref,0);	  
	}
      }
      else {
	// Evaluate expression with dynamic input & variable parameters
	UserVariable val = cur->exp->Evaluate(input);
	
	// Store the output data correctly
	StoreParameter(outofs,cur->token.evparam.dtype,&val);
      }
    }
    else
      printf("CONFIG: SetDynamicParameters: Unknown destination in token!\n");

    cur = cur->next;
  }
};

// Are the conditions in the EventBinding bind matched by the
// given input event and user variables?
char InputMatrix::CheckConditions(Event *input, 
				  EventBinding *bind) {
  char match = 1;

  // Check dynamic token conditions
  DynamicToken *cur = bind->tokenconds;
  while (cur != 0 && match) {
    // Compare evaluations of token and expression
    UserVariable cmp1;
    cur->token.Evaluate(&cmp1,input,1);
    UserVariable cmp2 = cur->exp->Evaluate(input);
    if (cmp1 != cmp2)
      match = 0;
    else
      cur = cur->next;
  }

  return match;
};

// Traverses through the list of event bindings beginning at 'start'
// looking for a binding that matches current user variables and input
// event 'ev'
EventBinding *InputMatrix::MatchBinding(Event *ev, EventBinding *start) {
#if 0
  {
    EventBinding *cur = start;
    int count = 0;
    while (cur != 0) {
      count++;
      cur = cur->next;
    }
    printf(" [SCAN] Hash size: %d",count);
  }
#endif

  EventBinding *cur = start;
  char go = 1, prevcont = 0;
  while (cur != 0 && go) {
    if (!prevcont && CheckConditions(ev,cur))
      go = 0;
    else {
      prevcont = cur->continued;
      cur = cur->next;
    }
  }

#if 0
  if (cur != 0)
    printf(" HIT!\n");
  else
    printf("\n");
#endif

  return cur;
};

void InputMatrix::ReceiveEvent(Event *ev, EventProducer *from) {
  char echo = 1;
  EventBinding *match = 0;

  // Input events
  int i = ev->GetType();

  if (ev->GetType() < T_EV_Last_Bindable) {
    EventHook *ev_hook = app->getCFG()->ev_hook;
    if (ev_hook == 0 || !ev_hook->HookEvent(ev,from)) {
      if (CRITTERS && ev->GetType() == T_EV_GoSub)
	printf("CONFIG: GoSub(%d)\n",((GoSubEvent *) ev)->sub);
      
      if (input_bind[i] != 0) {
	EventBinding **cur_hash = input_bind[i];
	
	// Find indexed parameter
	EventBinding *search = 0;
	EventParameter param;
	int pidx = Event::GetParamIdxByType((EventType) i);
	if (pidx == -1) {
	  //printf("noidx ");
	  search = *cur_hash;
	}
	else {
	  param = ev->GetParam(pidx);
	  if (param.dtype != T_int) {
	    // Error!
	    printf("CONFIG: Error: Indexed event parameters must be "
		   "integers!\n");
	  } else {
	    // Get value of indexed parameter in input event
	    char *evofs = (char *)ev + param.ofs;
	    int hashval = *((int *) evofs) % param.max_index;
	    
	    //printf("hashidx: %d ",hashval);
	    
	    // Search in the right list of bindings- based on the hash index
	    search = cur_hash[hashval];
	  }
	}
	
	// Now, check for matching binding in the search list
	if (search != 0)
	  match = MatchBinding(ev,search);
	if (match == 0 && pidx != -1 && cur_hash[param.max_index] != 0)
	  // OK, no match on the exact hash! -- check wildcards stored at the
	  // end of the hashtable
	  match = MatchBinding(ev,cur_hash[param.max_index]);
      }
      
      // First matching binding, check if she says to echo
      if (match != 0)
	echo = match->echo;
      else if (ev->GetType() == T_EV_GoSub)
	echo = 0; // Never echo back unmatched GoSub- we'll get into a loop
      
      // printf("CONFIG: Binding match %p (echo %d)\n",match,echo);
      
      while (match != 0) {
	// So we have a binding.. trigger the bound event!
	
	Event *shot = (Event *) match->boundproto->RTNew();
	if (shot == 0) 
	  printf("CONFIG: WARNING: Can't send event- RTNew() failed\n");
	else {
	  *shot = *match->boundproto; // Copy from prototype
	  SetDynamicParameters(ev,shot,match);
	  app->getEMG()->BroadcastEventNow(shot, this);
	}
	
	// Trigger any continued bindings..
	if (match->continued)
	  match = match->next;
	else
	  match = 0;
      }
      
      // Echo the incoming event back?
      if (echo) {
	Event *echo = (Event *) ev->RTNew();
	if (echo == 0) 
	  printf("CONFIG: WARNING: Can't send event- RTNew() failed\n");
	else {
	  *echo = *ev;    // Copy from incoming input
	  echo->echo = 1; // Set echo flag
	  app->getEMG()->BroadcastEventNow(echo, this);    
	}
      }
    } else if (ev_hook != 0) {
      // Hook swallowed event

      if (CRITTERS) 
	printf("CONFIG: Hook swallowed event (type %d).\n",ev->GetType());
    }
  } else {
    switch (ev->GetType()) {
      // *** Internal events
      
    case T_EV_SetVariable :
      {
	SetVariableEvent *sv = (SetVariableEvent *) ev;
	
	// Set the variable to the given value
	if (sv->var == 0 || sv->var->IsSystemVariable())
	  printf(" SetVariableEvent: Invalid variable!\n");
	else {
	  if (CRITTERS) {
	    printf("CONFIG: SetVariable: ");
	    sv->var->Print();
	    printf(" -> ");
	    
	    sv->var->SetFrom(sv->value);
	    
	    sv->var->Print();
	    printf("\n");
	  }
	  else
	    sv->var->SetFrom(sv->value);
	}
      }
      break;
      
    case T_EV_ToggleVariable :
      {
	ToggleVariableEvent *tv = (ToggleVariableEvent *) ev;
	
	// Set the variable to the given value
	if (tv->var == 0 || tv->var->IsSystemVariable())
	  printf(" ToggleVariableEvent: Invalid variable!\n");
	else {
	  int tmp = (int) (*(tv->var));
	  tmp++;
	  if (tmp > tv->maxvalue)
	    tmp = 0;
	  UserVariable v;
	  v.type = T_int;
	  v = tmp;
	  
	  if (CRITTERS) {
	    printf("CONFIG: ToggleVariable: ");
	    tv->var->Print();
	    printf(" -> ");
	    tv->var->SetFrom(v);
	    tv->var->Print();
	    printf("\n");
	  }
	  else 
	    tv->var->SetFrom(v);
	}
      }
      break;
      
    case T_EV_ShowDebugInfo :
      {
	ShowDebugInfoEvent *dev = (ShowDebugInfoEvent *) ev;
	app->getCFG()->showdebug = dev->show;
	
	if (CRITTERS)
	  printf("CONFIG: show debugging info = %s\n",
		 (dev->show ? "on" : "off"));
      }
      break;
      
    case T_EV_AdjustMidiTranspose :
      {
	AdjustMidiTransposeEvent *tev = (AdjustMidiTransposeEvent *) ev;
	app->getCFG()->transpose += tev->adjust;
	
	if (CRITTERS)
	  printf("CONFIG: adjust transpose midi by %d: transpose = %d.\n",
		 tev->adjust,app->getCFG()->transpose);
      }
      break;
    
    default:
      break;
    }
  }
};

void FloConfig::ConfigureEventBindings(xmlDocPtr doc, xmlNode *events) {
  xmlNode *cur_node;
  for (cur_node = events->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    // Check for a help node
    CheckForHelp(cur_node);

    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"declare"))) {
      // Variable declaration
      im.CreateVariable(cur_node);
    }
    else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"binding"))) {
      // Binding
      im.CreateBinding(cur_node);
    }
  }
};

void FloConfig::ConfigureGeneral(xmlDocPtr doc, xmlNode *gen) {
  xmlNode *cur_node;
  for (cur_node = gen->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    // Check for a help node
    CheckForHelp(cur_node);

    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"var"))) {
      // System variable setting

      // Check
      xmlChar *n = 0;
      if ((n = xmlGetProp(cur_node, (const xmlChar *)"numloopids")) != 
	  0) {
	num_triggers = atoi((char *) n);
	if (num_triggers < 1)
	  num_triggers = 1; 
	printf("CONFIG: Starting with %d triggers.\n",num_triggers);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"librarypath")) != 0) {
	if (xmlStrchr(n,'~') == n) {
	  // Reference to home dir
	  char *homedir = getenv("HOME");
	  librarypath = new char[strlen(homedir)+xmlStrlen(n)+1];
	  strcpy(librarypath,homedir);
	  strcat(librarypath,(char *) &n[1]);
	} else {
	  librarypath = new char[xmlStrlen(n)+1];
	  strcpy(librarypath,(char *) n);
	}

	char *ptr = strrchr(librarypath,'/');
	if (ptr != 0 && strlen(ptr) == 1) // Trailing backslash?
	  // Remove
	  *ptr = '\0';
	printf("CONFIG: Library path: '%s'\n",librarypath);
	
	// Ensure library path exists
	if (mkdir(librarypath,S_IRWXU)) {
	  if (errno != EEXIST) {
	    printf("DISK: Error %d creating library folder '%s'- "
		   "do you have write permission there?\n",
		   errno,librarypath);
	  }
	} else 
	  printf("DISK: Created new library @ '%s'\n",librarypath);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"maxplayvol")) != 0) {
	maxplayvol = atof((char *) n);
	if (maxplayvol < 0.0)
	  maxplayvol = 1.0;
	printf("CONFIG: Maximum play volume set to %.2f%%.\n",
	       maxplayvol*100);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"maxlimitergain")) != 0) {
	maxlimitergain = atof((char *) n);
	if (maxlimitergain < 0.0)
	  maxlimitergain = 1.0;
	printf("CONFIG: Maximum limiter gain set to %.2f%%.\n",
	       maxlimitergain*100);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"limiterthreshhold")) != 0) {
	limiterthreshhold = atof((char *) n);
	if (limiterthreshhold < 0.0)
	  limiterthreshhold = 0.9;
	printf("CONFIG: Limiter threshhold set to %.2f%%.\n",
	       limiterthreshhold*100);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"limiterreleaserate")) != 0) {
	limiterreleaserate = atof((char *) n);
	if (limiterreleaserate < 0.0)
	  limiterreleaserate = 0.000020;
	printf("CONFIG: Limiter release rate set to %.2f%%.\n",
	       limiterreleaserate*100);
      } else if ((n = xmlGetProp(cur_node,
                                 (const xmlChar *)"loopoutformat")) != 0) {
	loopoutformat = GetCodecFromName((const char *) n);
	if (loopoutformat == UNKNOWN)
	  printf(FWEELIN_ERROR_COLOR_ON
		 "CONFIG: Invalid loop output format: %s\n"
		 FWEELIN_ERROR_COLOR_OFF,n);
	else
	  printf("CONFIG: Loop out format is: %s\n", GetCodecName(loopoutformat));
      } else if ((n = xmlGetProp(cur_node,
				 (const xmlChar *)"streamoutformat")) != 0) {
	streamoutformat = GetCodecFromName((const char *) n);
	if (streamoutformat == UNKNOWN)
	  printf(FWEELIN_ERROR_COLOR_ON
		 "CONFIG: Invalid stream output format: %s\n"
		 FWEELIN_ERROR_COLOR_OFF,n);
	else
	  printf("CONFIG: Stream out format is: %s\n", 
		 GetCodecName(streamoutformat));
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"oggquality")) != 0) {
	float q = atof((char *) n);
	if (q <= 0.0)
	  printf(FWEELIN_ERROR_COLOR_ON
		 "CONFIG: Invalid OGG quality: %f\n"
		 FWEELIN_ERROR_COLOR_OFF,q);
	else {
	  vorbis_encode_quality = q;
	  printf("CONFIG: OGG Quality: %f\n",vorbis_encode_quality);
	}
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"midiouts")) != 0) {
	midiouts = atoi((char *) n);
	if (midiouts < 1)
	  midiouts = 1;
	printf("CONFIG: Config sets %d MIDI outputs.\n",midiouts);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"externalaudioinputs")) != 0) {
	// # of inputs
	extaudioins = xmlStrlen(n);
	if (extaudioins < 1)
	  extaudioins = 1;
	int intaudioins = AudioBuffers::GetIntAudioIns();
	printf("CONFIG: Starting with %d external audio input(s)\n"
	       "        and %d internal audio input(s)--\n",
	       extaudioins,intaudioins);
	ms_inputs = new char[extaudioins + intaudioins];

	// Assign stereo/mono and create input variables
	char tmp[255];
	for (int i = 0; i < extaudioins + intaudioins; i++) {
	  if (i < extaudioins) {
	    switch (n[i]) {
	    case 'S' :
	    case 's' :
	      ms_inputs[i] = 1;
	      printf("CONFIG: Input #%d is stereo\n",i+1);
	      break;
	      
	    case 'M' :
	    case 'm' :
	      ms_inputs[i] = 0;
	      printf("CONFIG: Input #%d is mono\n",i+1);
	      break;
	      
	    default:
	      printf(FWEELIN_ERROR_COLOR_ON
		     "*** INIT: WARNING: I don't understand input #%d = %c\n"
		     "Assuming input is mono.\n"
		     FWEELIN_ERROR_COLOR_OFF,
		     i+1,n[i]);
	      ms_inputs[i] = 0;
	      break;
	    }
	  } else
	    ms_inputs[i] = 0;
	    
	  snprintf(tmp,255,"SYSTEM_in_%d_volume",i+1);
	  AddEmptyVariable(tmp);
	  snprintf(tmp,255,"SYSTEM_in_%d_record",i+1);
	  AddEmptyVariable(tmp);
	  snprintf(tmp,255,"SYSTEM_in_%d_peak",i+1);
	  AddEmptyVariable(tmp);
	}
      } 

      if (n != 0)
	xmlFree(n);
    }
    
#if USE_FLUIDSYNTH
    else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"fluidsynth"))) {
      // FluidSynth setting

      // Check
      xmlChar *n = 0;
      if ((n = xmlGetProp(cur_node, (const xmlChar *)"param")) != 0) {
	xmlChar *val = 0;
	if ((val = xmlGetProp(cur_node, (const xmlChar *)"setint")) != 0) {
	  AddFluidParam(new FluidSynthParam_Int((char *)n,
						atoi((char *)val)));	  
	} else if ((val = xmlGetProp(cur_node, 
				     (const xmlChar *)"setnum")) != 0) {
	  AddFluidParam(new FluidSynthParam_Num((char *)n,
						atof((char *)val)));  
	} else if ((val = xmlGetProp(cur_node, 
				     (const xmlChar *)"setstr")) != 0) {
	  AddFluidParam(new FluidSynthParam_Str((char *)n,(char *)val));       
	}  	  
	
	if (val != 0)
	  xmlFree(val);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"soundfont")) != 0) {
	AddFluidFont(new FluidSynthSoundFont((char *)n));
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"interpolation")) != 0) {
	fsinterp = atoi((char *)n);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"channel")) != 0) {
	fschannel = atoi((char *)n);
      } else if ((n = xmlGetProp(cur_node, 
				 (const xmlChar *)"stereo")) != 0) {
	fsstereo = atoi((char *)n);

	if (ms_inputs != 0) {
	  ms_inputs[extaudioins] = fsstereo;

	  printf("CONFIG: FluidSynth running in %s\n",
		 (fsstereo ? "stereo" : "mono"));
	} else
	  printf(FWEELIN_ERROR_COLOR_ON
		 "*** INIT: WARNING: FluidSynth stereo can't be set before "
		 "externalaudioinputs is set- check config!\n"
		 FWEELIN_ERROR_COLOR_OFF);
      } 
      
      if (n != 0)
	xmlFree(n);
    }
#endif
  }

  // Print whether master is stereo
  printf("CONFIG: FreeWheeling is running in %s",
	 (IsStereoMaster() ? "stereo.\n        * Please be aware, this "
	  "significantly increases memory usage. *\n" : "mono.\n"));
};

char FloConfig::IsStereoMaster() {
  // Check inputs
  int numins = extaudioins + AudioBuffers::GetIntAudioIns();
  for (int i = 0; i < numins; i++)
    if (ms_inputs[i])
      return 1; // Yep, one input is stereo, so we have to run in stereo
  
  // No stereo inputs, run in mono!
  return 0;
};

// Extracts an array of floats (delimited by character delim_char)
// from the given string- returns size of array in 'size'
float *FloConfig::ExtractArray(char *n, int *size, char delim_char) {
  char buf[255];
  strncpy(buf,n,254);
  buf[254] = '\0';

  char *delim = buf;
  *size = 0;
  while (delim != 0) {
    (*size)++;
    delim = strchr(delim+1,delim_char);
  }

  if (*size == 0)
    return 0;

  float *array = new float[*size];

  delim = buf;
  int i = 0;
  while (i < *size) {
    char *nd = strchr(delim,delim_char);
    if (nd != 0) {
      *nd = '\0';
      nd++;
    }
    //printf("%d: %s\n",i,delim);

    array[i++] = atof(delim);
    delim = nd;
  }

  return array;
};

void FloConfig::ConfigureElement(xmlDocPtr doc, xmlNode *elemn, 
				 FloLayoutElement *elem, float xscale,
				 float yscale) {
  xmlNode *cur_node;
  for (cur_node = elemn->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    // Check for a help node
    CheckForHelp(cur_node);

    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"box"))) {
      // Box declaration
      FloLayoutBox *nw = new FloLayoutBox();
      printf("    Box: ");
      
      xmlChar *n = xmlGetProp(cur_node, (const xmlChar *)"outline");
      if (n != 0) {
	nw->lineleft = (strchr((char *)n,'L') ? 1 : 0);
	nw->linetop = (strchr((char *)n,'T') ? 1 : 0);
	nw->lineright = (strchr((char *)n,'R') ? 1 : 0);
	nw->linebottom = (strchr((char *)n,'B') ? 1 : 0);
	printf("outline (L%d,T%d,R%d,B%d) ",nw->lineleft,nw->linetop,
	       nw->lineright,nw->linebottom);
	xmlFree(n);
      }

      // Position
      n = xmlGetProp(cur_node, (const xmlChar *)"pos");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs >= 4) {
	  nw->left = (int) round(elem->bx + XCvtf(xscale*coord[0]));
	  nw->top = (int) round(elem->by + YCvtf(yscale*coord[1]));
	  nw->right = (int) round(elem->bx + XCvtf(xscale*coord[2]));
	  nw->bottom = (int) round(elem->by + YCvtf(yscale*coord[3]));
	  printf("dim (%d,%d)-(%d,%d) ",nw->left,nw->top,
		 nw->right,nw->bottom);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Link in the new geometry
      if (elem->geo == 0) 
	elem->geo = nw;
      else {
	FloLayoutElementGeometry *cur = elem->geo;
	while (cur->next != 0)
	  cur = cur->next;
	cur->next = nw;
      }
      printf("\n");
    }
  }
};

void FloConfig::ConfigureLayout(xmlDocPtr doc, xmlNode *layn, 
			    FloLayout *lay, float xscale,
			    float yscale) {
  xmlNode *cur_node;
  for (cur_node = layn->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    // Check for a help node
    CheckForHelp(cur_node);

    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"element"))) {
      // Element declaration
      FloLayoutElement *nw = new FloLayoutElement();
      printf("  Element: ");

      xmlChar *n = xmlGetProp(cur_node, (const xmlChar *)"id");
      if (n != 0) {
	nw->id = atoi((char *)n);
	printf("#%d ",nw->id);
	xmlFree(n);
      }
      
      n = xmlGetProp(cur_node, (const xmlChar *)"name");
      if (n != 0) {
	nw->name = new char[xmlStrlen(n)+1];
	strcpy(nw->name,(char*)n);
	printf("'%s' ",nw->name);
	xmlFree(n);
      }

      // Base
      n = xmlGetProp(cur_node, (const xmlChar *)"base");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  nw->bx = lay->xpos + XCvtf(xscale*coord[0]);
	  nw->by = lay->ypos + YCvtf(yscale*coord[1]);
	  printf("@ (%d,%d) ",(int) nw->bx,(int) nw->by);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Name position
      n = xmlGetProp(cur_node, (const xmlChar *)"namepos");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  nw->nxpos = (int) round(nw->bx + XCvtf(xscale*coord[0]));
	  nw->nypos = (int) round(nw->by + YCvtf(yscale*coord[1]));
	  printf("Name@ (%d,%d) ",nw->nxpos,nw->nypos);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Loop position
      n = xmlGetProp(cur_node, (const xmlChar *)"looppos");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  nw->loopx = (int) round(nw->bx + XCvtf(xscale*coord[0]));
	  nw->loopy = (int) round(nw->by + YCvtf(yscale*coord[1]));
	  printf("looppos (%d,%d) ",nw->loopx,nw->loopy);
	  delete[] coord;
	}
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"loopsize");
      if (n != 0) {
	nw->loopsize = MIN(XCvt(MIN(xscale,yscale)*atof((char *)n)),
			   YCvt(MIN(xscale,yscale)*atof((char *)n)));
	printf("loopsize %d ",nw->loopsize);
	xmlFree(n);
      }

      // Link in the new element
      if (lay->elems == 0) 
	lay->elems = nw;
      else {
	FloLayoutElement *cur = lay->elems;
	while (cur->next != 0)
	  cur = cur->next;
	cur->next = nw;
      }
      printf("\n");

      // Now populate the element with geometries..
      ConfigureElement(doc,cur_node,nw,xscale,yscale);
    }
  }
};

void FloConfig::ConfigurePatchBanks(xmlNode *pb, PatchBrowser *br) {
  // The patch browser can contain a number of patchbanks.
  // Configure them.
  for (xmlNode *cur_node = pb->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"patchbank"))) {
      // Get MIDI port
      int pb_mport = 0;
      xmlChar *n = 0;
      if ((n = xmlGetProp(cur_node, (const xmlChar *)"midiport")) != 0) {
	pb_mport = atoi((char *) n);
	xmlFree(n);
      }

      // Separate channels into distinct patchbanks?
      char sepchan = 0;
      if ((n = xmlGetProp(cur_node, 
			  (const xmlChar *)"separatechannels")) != 0) {
	sepchan = atoi((char *) n);
	xmlFree(n);
      }

      // Suppress bank/program changes when changing patches? 
      // (ie only change channel)
      char suppresschg = 0;
      if ((n = xmlGetProp(cur_node, 
			  (const xmlChar *)"suppressprogramchanges")) != 0) {
	suppresschg = atoi((char *) n);
	xmlFree(n);
      }

      xmlChar *pb_patches = 
	xmlGetProp(cur_node, (const xmlChar *)"patches");
      if (pb_patches != 0) {
	// PatchBank file defined
	xmlDocPtr pb_doc;
	char *homedir = getenv("HOME");
	char buf[strlen(FWEELIN_DATADIR) + 
		 strlen(homedir) + strlen(FWEELIN_CONFIG_DIR) + 
		 xmlStrlen(pb_patches) + 4];

	// Try both ~/.fweelin and fweelin data dir
	char notfound = 0;
	sprintf(buf,"%s/%s/%s",homedir,FWEELIN_CONFIG_DIR,(char *) pb_patches);
	struct stat st;

	if (stat(buf,&st) != 0) {
	  // Can't find it there. Try data dir
	  sprintf(buf,"%s/%s",FWEELIN_DATADIR,(char *) pb_patches);
	  if (stat(buf,&st) != 0) {
	    // Not in either place!
	    printf(FWEELIN_ERROR_COLOR_ON
		   "\n*** INIT: ERROR: Can't find patches file '%s' in "
		   "folders:\n\n%s/%s\n%s\n"
		   FWEELIN_ERROR_COLOR_OFF,
		   pb_patches,homedir,FWEELIN_CONFIG_DIR,FWEELIN_DATADIR);
	    notfound = 1;
	  }
	}

	// Load and parse
	pb_doc = (notfound ? 0 : xmlParseFile(buf));
	if (pb_doc == 0)
	  printf(FWEELIN_ERROR_COLOR_ON
		 "\n*** INIT: ERROR: Problem parsing patches file '%s'.\n"
		 FWEELIN_ERROR_COLOR_OFF,
		 pb_patches);
	else {
	  xmlNode *pb_root = xmlDocGetRootElement(pb_doc);
	  if (!pb_root || !pb_root->name ||
	      xmlStrcmp(pb_root->name,(const xmlChar *) "patchlist"))
	    printf(FWEELIN_ERROR_COLOR_ON
		   "\n*** INIT: ERROR: Patches file '%s' invalid format.\n"
		   FWEELIN_ERROR_COLOR_OFF,
		   pb_patches);
	  else {
	    int p_curchan = -1,
	      pcnt = 0;
	    for (xmlNode *pb_curpatch = pb_root->children; pb_curpatch != NULL;
		 pb_curpatch = pb_curpatch->next) {
	      if ((!xmlStrcmp(pb_curpatch->name, (const xmlChar *)"combi"))) {
		// Combi (multi-zone multi-channel)

		// First, count zones
		int numzones = 0;
		for (xmlNode *curzone = pb_curpatch->children; curzone != NULL;
		     curzone = curzone->next)
		  if ((!xmlStrcmp(curzone->name, (const xmlChar *)"zone")))
		    numzones++;

		printf("\nCOMBI: Numzones: %d\n",numzones);

		// Setup combi
		xmlChar *cname = 0;
		cname = xmlGetProp(pb_curpatch,(const xmlChar *)"name");

		if (pcnt == 0) {
		  // First patch
		  // Create new PatchBank
		  br->PB_Add(new PatchBank(pb_mport,suppresschg));
		}
		  
		PatchItem *pi = 
		  new PatchItem(pcnt++,-1,-1,-1,(char *) cname);
		pi->SetupZones(numzones);
		br->AddItem(pi);
		
		if (pcnt % PatchBrowser::DIV_SPACING == 0)
		  br->AddItem(new BrowserDivision());

		if (cname != 0)
		  xmlFree(cname);
		
		// For each zone
		int curzone_idx = 0;
		for (xmlNode *curzone = pb_curpatch->children; curzone != NULL;
		     curzone = curzone->next) {
		  if ((!xmlStrcmp(curzone->name, (const xmlChar *)"zone"))) {
		    int kr_lo = 0,
		      kr_hi = 0;
		    if ((n = xmlGetProp(curzone, 
					(const xmlChar *)"keyrange")) != 0) {
		      int krs; 
		      float *kr = ExtractArray((char *)n, &krs, '>');
		      if (krs == 2) {
			kr_lo = (int) kr[0];
			kr_hi = (int) kr[1];
		      }
		      delete[] kr;
		      xmlFree(n);
		    }
		    
		    char mport_r = 0;
		    int mport = pb_mport;
		    xmlChar *n = 0;
		    if ((n = xmlGetProp(curzone, 
					(const xmlChar *)"midiport")) != 0) {
		      mport_r = 1;
		      mport = atoi((char *) n);
		      xmlFree(n);
		    }

		    int bank = -1;
		    if ((n = xmlGetProp(curzone, 
					(const xmlChar *)"bank")) != 0) {
		      bank = atoi((char *) n);
		      xmlFree(n);
		    }		
		    
		    int prog = -1;
		    if ((n = xmlGetProp(curzone, 
					(const xmlChar *)"program")) != 0) {
		      prog = atoi((char *) n);
		      xmlFree(n);
		    }

		    int chan = 0;
		    if ((n = xmlGetProp(curzone,
					(const xmlChar *)"channel")) != 0) {
		      chan = atoi((char *) n);
		      xmlFree(n);
		    }

		    printf("  ZONE [%d>%d]: midiport[%s]:%d "
			   "bank: %d prog: %d chan: %d\n",
			   kr_lo,kr_hi,
			   (mport_r ? "REDIRECT" : "DEFAULT"),mport,
			   bank,prog,chan);

		    pi->GetZone(curzone_idx)->SetupZone(kr_lo,kr_hi,
							mport_r,mport,
							bank,prog,chan);
		    curzone_idx++;
		  }
		}
	      } else if ((!xmlStrcmp(pb_curpatch->name, 
				     (const xmlChar *)"patch"))) {
		// Single channel patch
		int chan = 0;
		if ((n = xmlGetProp(pb_curpatch,
				    (const xmlChar *)"channel")) != 0) {
		  chan = atoi((char *) n);
		  xmlFree(n);
		}

		int bank = 0;
		if ((n = xmlGetProp(pb_curpatch, 
				    (const xmlChar *)"bank")) != 0) {
		  bank = atoi((char *) n);
		  xmlFree(n);
		}		

		int prog = 0;
		if ((n = xmlGetProp(pb_curpatch, 
				    (const xmlChar *)"program")) != 0) {
		  prog = atoi((char *) n);
		  xmlFree(n);
		}		

		xmlChar *pname = 0;
		if ((pname = xmlGetProp(pb_curpatch, 
					(const xmlChar *)"name")) != 0) {
		  if ((sepchan && chan != p_curchan) ||
		      (!sepchan && pcnt == 0)) {
		    // Separate channels (patchbank for each channel)-- 
		    // or first patch 

		    // Create new PatchBank
		    br->PB_Add(new PatchBank(pb_mport,suppresschg));
		    p_curchan = chan;
		    pcnt = 0;
		    //printf("new patchbank: mport: %d chan: %d\n",
		    //	     pb_mport,chan);
		  }
		  
		  br->AddItem(new PatchItem(pcnt++,bank,prog,chan,
					    (char *) pname));
		  //printf("bank: %d prog: %d patch: '%s'\n",bank,prog,
		  //	   (char *) pname);

		  if (pcnt % PatchBrowser::DIV_SPACING == 0)
		    br->AddItem(new BrowserDivision());

		  xmlFree(pname);
		}		
	      }
	    }
	  }

	  xmlFreeDoc(pb_doc);
	}

	xmlFree(pb_patches);
      }
    }
  }    
};

void FloConfig::ConfigureVideo(xmlDocPtr doc, xmlNode *vid) {
  xmlNode *cur_node;
  for (cur_node = vid->children; cur_node != NULL; 
       cur_node = cur_node->next) {
    // Check for a help node
    CheckForHelp(cur_node);

    if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"var"))) {
      // System variable setting

      // Check
      xmlChar *n = 0;
      if ((n = xmlGetProp(cur_node, (const xmlChar *)"resolution")) != 
	  0) {
	// Video resolution
	int cs;
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  vsize[0] = (int)coord[0];
	  vsize[1] = (int)coord[1];
	  scope_sample_len = vsize[0]; // Scope goes across screen
	  delete[] coord;
	}

	printf("CONFIG: Starting with (%d,%d) resolution.\n",vsize[0],
	       vsize[1]);
	
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"videodelay");
      if (n != 0) {
	vdelay = atoi((char *)n);
	printf("CONFIG: Video delay: %d ms\n",vdelay);
	vdelay *= 1000; // usecs
	xmlFree(n);
      }
    } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"font"))) {
      // Font loading
      FloFont *nw = new FloFont();
      printf("CONFIG: New onscreen font: ");

      xmlChar *n = xmlGetProp(cur_node, (const xmlChar *)"name");
      if (n != 0) {
	nw->name = new char[xmlStrlen(n)+1];
	strcpy(nw->name,(char*)n);
	printf("%s: ",nw->name);
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"file");
      if (n != 0) {
	nw->filename = new char[xmlStrlen(n)+1];
	strcpy(nw->filename,(char*)n);
	printf(nw->filename);
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"size");
      if (n != 0) {
	nw->size = atoi((char *)n);
	printf(" (%d pt)",nw->size);
	xmlFree(n);
      }

      // Link in the new font
      if (fonts == 0) 
	fonts = nw;
      else {
	FloFont *cur = fonts;
	while (cur->next != 0)
	  cur = cur->next;
	cur->next = nw;
      }
      printf("\n");
    } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"display"))) {
      // Onscreen display declaration
      FloDisplay *nw = 0;
      printf("CONFIG: New onscreen display: ");

      // Type of display?
      xmlChar *n = xmlGetProp(cur_node, (const xmlChar *)"type");
      if (n != 0) {
	if (!xmlStrcmp(n, (const xmlChar *)"text")) {
	  printf("(text) ");
	  nw = new FloDisplayText();
	} else if (!xmlStrcmp(n, (const xmlChar *)"browser")) {
	  printf("(browser) ");

	  // Browser type
	  BrowserItemType btype = (BrowserItemType) 0;
	  xmlChar *nn = xmlGetProp(cur_node, (const xmlChar *)"browsetype");
	  if (nn != 0) {
	    ParsedExpression *tmp = im.ParseExpression((char *) nn, 0);
	    btype = (BrowserItemType) (int) tmp->Evaluate(0);
	    printf("type: %s ",Browser::GetTypeName(btype));
	    delete tmp;
	    xmlFree(nn);
	  }

	  // Expanded browser view
	  char xpand = 0;
	  int xpand_x1 = 0,
	    xpand_x2 = 0,
	    xpand_y1 = 0,
	    xpand_y2 = 0;
	  float xpand_delay = 1.0;
	  nn = xmlGetProp(cur_node, (const xmlChar *)"xpand");
	  if (nn != 0) {
	    xpand = atoi((char *) nn);
	    printf("xpand: %d ",xpand);
	    xmlFree(nn);
	  }
	  nn = xmlGetProp(cur_node, (const xmlChar *)"xdelay");
	  if (nn != 0) {
	    xpand_delay = atof((char *) nn);
	    printf("xdelay: %f ",xpand_delay);
	    xmlFree(nn);
	  }
	  nn = xmlGetProp(cur_node, (const xmlChar *)"xbox");
	  if (nn != 0) {
	    int cs; 
	    float *coord = ExtractArray((char *)nn, &cs);
	    if (cs >= 4) {
	      xpand_x1 = (int) round(XCvtf(coord[0]));
	      xpand_y1 = (int) round(YCvtf(coord[1]));
	      xpand_x2 = (int) round(XCvtf(coord[2]));
	      xpand_y2 = (int) round(YCvtf(coord[3]));
	      printf("xpand_box: (%d,%d)-(%d,%d) ",xpand_x1,xpand_y1,
		     xpand_x2,xpand_y2);
	      delete[] coord;
	    }
	    xmlFree(nn);
	  }

	  // Check for another browser with the same type-- not allowed
	  FloDisplay *checkd = displays;
	  char dupe = 0;
	  while (!dupe && checkd != 0) {
	    if (checkd->GetFloDisplayType() == FD_Browser &&
		((Browser *) checkd)->GetType() == btype)
	      dupe = 1;
	    checkd = checkd->next;
	  }

	  if (!dupe) {
	    switch (btype) {
	    case B_Loop_Tray :
	      {
		int loopsize = 0;
		nn = xmlGetProp(cur_node, (const xmlChar *)"loopsize");
		if (nn != 0) {
		  loopsize = (int) round(XCvtf(atof((char *) nn)));
		  xmlFree(nn);
		}
		
		nw = new LoopTray(btype,xpand,xpand_x1,xpand_y1,
				  xpand_x2,xpand_y2,loopsize);
	      }
	      break;
	      
	    case B_Scene_Tray :
	      {
	      }
	      break;

	    case B_Patch :
	      {
		nw = new PatchBrowser(btype,xpand,xpand_x1,xpand_y1,
				      xpand_x2,xpand_y2,xpand_delay);
		ConfigurePatchBanks(cur_node,(PatchBrowser *) nw);
	      }
	      break;

	    default :
	      {
		// All other kinds of browsers
		nw = new Browser(btype,xpand,xpand_x1,xpand_y1,
				 xpand_x2,xpand_y2,xpand_delay);
	      }
	      break;
	    }
	  } else {
	    printf(FWEELIN_ERROR_COLOR_ON
		   "\n*** INIT: WARNING: Duplicate browser of type: '%s'\n"
		   FWEELIN_ERROR_COLOR_OFF,
		   Browser::GetTypeName(btype));
	    nw = 0;
	  }
	} else if (!xmlStrcmp(n, (const xmlChar *)"switch")) {
	  printf("(switch) ");
	  nw = new FloDisplaySwitch();
	} else if (!xmlStrcmp(n, (const xmlChar *)"circle-switch")) {
	  printf("(circle-switch) ");
	  nw = new FloDisplayCircleSwitch();

	  // Circle radii
	  xmlChar *nn = xmlGetProp(cur_node, (const xmlChar *)"size1");
	  if (nn != 0) {
	    float sz = atof((char *)nn);
	    ((FloDisplayCircleSwitch *) nw)->rad1 = XCvt(sz);
	    printf("size1 %d ",((FloDisplayCircleSwitch *) nw)->rad1);
	    xmlFree(nn);
	  } 
	  nn = xmlGetProp(cur_node, (const xmlChar *)"size0");
	  if (nn != 0) {
	    float sz = atof((char *)nn);
	    ((FloDisplayCircleSwitch *) nw)->rad0 = XCvt(sz);
	    printf("size0 %d ",((FloDisplayCircleSwitch *) nw)->rad0);
	    xmlFree(nn);
	  } 

	  // Flashing?
	  nn = xmlGetProp(cur_node, (const xmlChar *)"flash");
	  if (nn != 0) {
	    ((FloDisplayCircleSwitch *) nw)->flash = atoi((char *)nn);
	    if (((FloDisplayCircleSwitch *) nw)->flash)
	      printf("(flashing) ");
	    xmlFree(nn);
	  } 
	} else if (!xmlStrcmp(n, (const xmlChar *)"text-switch")) {
	  printf("(text-switch) ");
	  nw = new FloDisplayTextSwitch();

	  // Text lines
	  xmlChar *nn = xmlGetProp(cur_node, (const xmlChar *)"text1");
	  if (nn != 0) {
	    char *text1 = new char[xmlStrlen(nn)+1];
	    strcpy(text1,(char*)nn);
	    ((FloDisplayTextSwitch *) nw)->text1 = text1;
	    printf("'%s' ",text1);

	    xmlFree(nn);
	  } 
	  nn = xmlGetProp(cur_node, (const xmlChar *)"text0");
	  if (nn != 0) {
	    char *text0 = new char[xmlStrlen(nn)+1];
	    strcpy(text0,(char*)nn);
	    ((FloDisplayTextSwitch *) nw)->text0 = text0;
	    printf("'%s' ",text0);

	    xmlFree(nn);
	  } 
 	} else if (!xmlStrcmp(n, (const xmlChar *)"bar") || 
		   !xmlStrcmp(n, (const xmlChar *)"bar-switch")) {
	  // Bar or bar-switch?
	  char sw = 0;
	  FloDisplayBar *nwb;
	  if (!xmlStrcmp(n, (const xmlChar *)"bar-switch")) {
	    sw = 1;
	    printf("(bar-switch) ");
	    nwb = new FloDisplayBarSwitch();
	  } else {
	    printf("(bar) ");
	    nwb = new FloDisplayBar();
	  }

	  nw = nwb;

	  // Bar orientation
	  xmlChar *nn = xmlGetProp(cur_node, (const xmlChar *)"orientation");
	  if (nn != 0) {
	    if (!xmlStrcmp(nn,(const xmlChar *)"horizontal")) {
	      nwb->orient = O_Horizontal;
	      printf("(horizontal) ");
	    }
	    else if (!xmlStrcmp(nn,(const xmlChar *)"vertical")) {
	      nwb->orient = O_Vertical;
	      printf("(vertical) ");
	    }
	    else
	      printf("(invalid bar orient: '%s') ",nn);
	    xmlFree(nn);
	  }

	  // Bar scale
	  nn = xmlGetProp(cur_node, (const xmlChar *)"barscale");
	  if (nn != 0) {
	    nwb->barscale = atof((char *)nn);
	    xmlFree(nn);
	  }
	  nwb->barscale = (nwb->orient == O_Horizontal ? 
			   XCvtf(nwb->barscale) : YCvtf(nwb->barscale));
	  printf("barscale %.2f ",nwb->barscale);

	  // Bar thickness
	  nn = xmlGetProp(cur_node, (const xmlChar *)"thickness");
	  if (nn != 0) {
	    float bt = atof((char *)nn);
	    nwb->thickness = (nwb->orient == O_Horizontal ? YCvt(bt) : XCvt(bt));
	    printf("thickness %d ",nwb->thickness);
	    xmlFree(nn);
	  } 

	  if (sw) {
	    // Color
	    nn = xmlGetProp(cur_node, (const xmlChar *)"color");
	    if (nn != 0) {
	      ((FloDisplayBarSwitch *) nwb)->color = atoi((char *) nn);
	      printf(" color %d ",((FloDisplayBarSwitch *) nwb)->color);
	      xmlFree(nn);
	    }

	    // Calibration mark
	    nn = xmlGetProp(cur_node, (const xmlChar *)"calibrate");
	    if (nn != 0) {
	      ((FloDisplayBarSwitch *) nwb)->calibrate = 1;
	      ((FloDisplayBarSwitch *) nwb)->cval = atof((char *) nn);
	      printf(" calibrate %.2f ",((FloDisplayBarSwitch *) nwb)->cval);
	      xmlFree(nn);
	    }

	    // Expression to display
	    nn = xmlGetProp(cur_node, (const xmlChar *)"switchvar");
	    if (nn != 0) {
	      printf(" switch-expression: ");
	      ((FloDisplayBarSwitch *) nwb)->switchexp = im.ParseExpression((char *) nn, 0);
	      ((FloDisplayBarSwitch *) nwb)->switchexp->Print();
	      printf(" ");
	      xmlFree(nn);
	    }
	  }
	} else {
	  printf("(invalid display type: '%s')\n",n);
	}
	xmlFree(n);
      }

      if (nw != 0) {
	// Title
	n = xmlGetProp(cur_node, (const xmlChar *)"title");
	if (n != 0) {
	  nw->title = new char[xmlStrlen(n)+1];
	  strcpy(nw->title,(char*)n);
	  printf("'%s' ",nw->title);
	  xmlFree(n);
	}      
	
	// Position
	n = xmlGetProp(cur_node, (const xmlChar *)"pos");
	if (n != 0) {
	  int cs; 
	  float *coord = ExtractArray((char *)n, &cs);
	  if (cs) {
	    nw->xpos = XCvt(coord[0]);
	    nw->ypos = YCvt(coord[1]);
	    printf("@ (%d,%d) ",nw->xpos,nw->ypos);
	    delete[] coord;
	  }
	  xmlFree(n);
	}

	// Show?
	n = xmlGetProp(cur_node, (const xmlChar *)"show");
	if (n != 0) {
	  nw->show = atoi((char *)n);
	  if (nw->show)
	    printf("(show) ");
	  xmlFree(n);
	}

	// Font for display
	n = xmlGetProp(cur_node, (const xmlChar *)"font");
	if (n != 0) {
	  nw->font = GetFont((char *) n);
	  if (nw->font != 0)
	    printf("(font: %s) ",nw->font->name);
	  else 
	    printf("(ERR: no font named '%s'!) ",n);
	  xmlFree(n);
	}

	// ID
	n = xmlGetProp(cur_node, (const xmlChar *)"id");
	if (n != 0) {
	  ParsedExpression *tmp = im.ParseExpression((char *) n, 0);
	  nw->id = (int) tmp->Evaluate(0);
	  printf("(id: %d) ",nw->id);
	  delete tmp;
	  xmlFree(n);
	}

	// Expression to display
	n = xmlGetProp(cur_node, (const xmlChar *)"var");
	if (n != 0) {
	  printf("\n   expression: ");
	  nw->exp = im.ParseExpression((char *) n, 0);
	  nw->exp->Print();
	  xmlFree(n);
	}

	// Link in the new display
	if (displays == 0) 
	  displays = nw;
	else {
	  FloDisplay *cur = displays;
	  while (cur->next != 0)
	    cur = cur->next;
	  cur->next = nw;
	}
	printf("\n");
      }
    } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"layout"))) {
      // Layout declaration
      FloLayout *nw = new FloLayout();
      printf("CONFIG: New onscreen loop layout: ");
      
      xmlChar *n = xmlGetProp(cur_node, (const xmlChar *)"id");
      if (n != 0) {
	nw->id = atoi((char *)n);
	printf("#%d ",nw->id);
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"show");
      if (n != 0) {
	nw->show = atoi((char *)n);
	if (nw->show)
	  printf("(show) ");
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"label");
      if (n != 0) {
	nw->showlabel = atoi((char *)n);
	if (nw->showlabel)
	  printf("(label) ");
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"elabel");
      if (n != 0) {
	nw->showelabel = atoi((char *)n);
	if (nw->showelabel)
	  printf("(label elements) ");
	xmlFree(n);
      }

      n = xmlGetProp(cur_node, (const xmlChar *)"name");
      if (n != 0) {
	nw->name = new char[xmlStrlen(n)+1];
	strcpy(nw->name,(char*)n);;
	printf("'%s' ",nw->name);
	xmlFree(n);
      }

      float xscale = 1.0, yscale = 1.0; 
      n = xmlGetProp(cur_node, (const xmlChar *)"scale");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  xscale = coord[0];
	  yscale = coord[1];
	  printf("scale (%.2f,%.2f) ",xscale,yscale);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Position
      n = xmlGetProp(cur_node, (const xmlChar *)"pos");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  nw->xpos = XCvt(coord[0]);
	  nw->ypos = YCvt(coord[1]);
	  printf("@ (%d,%d) ",nw->xpos,nw->ypos);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Name position
      n = xmlGetProp(cur_node, (const xmlChar *)"namepos");
      if (n != 0) {
	int cs; 
	float *coord = ExtractArray((char *)n, &cs);
	if (cs) {
	  nw->nxpos = nw->xpos + XCvt(coord[0]);
	  nw->nypos = nw->ypos + YCvt(coord[1]);
	  printf("Name@ (%d,%d) ",nw->nxpos,nw->nypos);
	  delete[] coord;
	}
	xmlFree(n);
      }

      // Link in the new layout
      if (layouts == 0) 
	layouts = nw;
      else {
	FloLayout *cur = layouts;
	while (cur->next != 0)
	  cur = cur->next;
	cur->next = nw;
      }
      printf("\n");

      // Now populate the layout with elements..
      ConfigureLayout(doc,cur_node,nw,xscale,yscale);
    }
  }
};

// Is node 'n' a comment with help information? If so, add to our
// internal help list
void FloConfig::CheckForHelp(xmlNode *n) {
  if (n->type == XML_COMMENT_NODE) {
    char *str = (char *) n->content;
    while (*str == ' ')
      str++; // Advance past beginning whitespace
    if (!strncmp(str,FWEELIN_CONFIG_HELP_TOKEN,
		 strlen(FWEELIN_CONFIG_HELP_TOKEN))) {
      // This token is a comment with help prefix-- store
      str += strlen(FWEELIN_CONFIG_HELP_TOKEN);
      char *s = new char[strlen(str)+1];
      strcpy(s,str);

      // Replace divider with null to split string into two parts (columns)
      // This allows keys to be listed in separate help columns from their 
      // functions
      char *div = strchr(s,':');
      if (div != 0) {
	*div = '\0';
	div++;
      }

      FloStringList *nw = new FloStringList(s,div);
      printf(" add user help: %s:%s\n",s,div);

      // Link in the new string
      if (help == 0) 
	help = nw;
      else {
	FloStringList *cur = help;
	while (cur->next != 0)
	  cur = cur->next;
	cur->next = nw;
      }
    }
  }  
};

// Creates an empty variable based on the given name. The config file
// can then refer to the variable
UserVariable *FloConfig::AddEmptyVariable(char *name) {
  UserVariable *nw = new UserVariable();
  if (name != 0) {
    nw->name = new char[strlen(name)+1];
    strcpy(nw->name,name);
  }

  // Insert into variable list
  nw->next = im.vars;
  im.vars = nw;
  
  return nw;
};

// Makes the given variable into a system variable by linking it to
// the pointer
void FloConfig::LinkSystemVariable(char *name, CoreDataType type, char *ptr) {
  UserVariable *cur = im.vars;
  while (cur != 0) {
    if (cur->name != 0) {
      if (!strcmp(cur->name,name)) {
	// Variable found!- Link it with a system variable
	printf("CONFIG: Link system variable: %s -> %p\n",name,ptr);
	cur->type = type;
	cur->value = ptr;
      }
    }

    cur = cur->next;
  }
};

FloConfig::~FloConfig() 
{
  // Erase displays
  {
    FloDisplay *cur = displays;
    while (cur != 0) {
      FloDisplay *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }

  // Erase fonts
  {
    FloFont *cur = fonts;
    while (cur != 0) {
      FloFont *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }

  // Erase layouts
  {
    FloLayout *cur = layouts;
    while (cur != 0) {
      FloLayout *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }

  // Erase help
  {
    FloStringList *cur = help;
    while (cur != 0) {
      FloStringList *tmp = cur->next;
      delete cur;
      cur = tmp;
    }  
  }

#if USE_FLUIDSYNTH
  // Erase FluidSynth config
  {
    FluidSynthParam *cur = fsparam;
    while (cur != 0) {
      FluidSynthParam *tmp = cur->next;
      delete cur;
      cur = tmp;
    }  
  }
  {
    FluidSynthSoundFont *cur = fsfont;
    while (cur != 0) {
      FluidSynthSoundFont *tmp = cur->next;
      delete cur;
      cur = tmp;
    }  
  }
#endif

  if (ms_inputs != 0)
    delete[] ms_inputs;

  if (librarypath != 0)
    delete librarypath;
};

FloConfig::FloConfig(Fweelin *app) : im(app), 
  
  ev_hook(0), librarypath(0), midiouts(1),

  ms_inputs(0), extaudioins(0), 

  maxplayvol(5.0), maxlimitergain(1.0), limiterthreshhold(0.9), 
  limiterreleaserate(0.000020),

  loopoutformat(VORBIS), streamoutformat(VORBIS),
  vorbis_encode_quality(0.5),

  num_triggers(1024), vdelay(50000), showdebug(0), 
  layouts(0), fonts(0), displays(0), help(0),
		
#if USE_FLUIDSYNTH
  fsinterp(4), fschannel(0), fsstereo(1), fsparam(0), fsfont(0),
#endif
				     
  transpose(0), 
  
  loop_peaksavgs_chunksize(500), status_report(0) { 
  vsize[0] = 640;
  vsize[1] = 480;
  scope_sample_len = vsize[0]; // Scope goes across screen
};

// Parse configuration file, setup config
void FloConfig::Parse() {  
#ifdef __MACOSX__
  // On Mac OS X, Set FWEELIN_DATADIR to bundle resource folder
  
  CFURLRef url = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
  CFURLGetFileSystemRepresentation(url, true, (UInt8 *)FWEELIN_DATADIR, MAXPATHLEN);
  CFRelease(url);
#endif
  
  // Setup event type table
  Event::SetupEventTypeTable(im.app->getMMG());
  xmlDocPtr doc;
  char *homedir = getenv("HOME");
  char buf[strlen(FWEELIN_DATADIR) + 2*strlen(homedir) + 
	   strlen(FWEELIN_CONFIG_DIR) + strlen(FWEELIN_CONFIG_FILE) + 11];
  sprintf(buf,"%s/%s/%s",homedir,FWEELIN_CONFIG_DIR,FWEELIN_CONFIG_FILE);

  // Look for config file
  char pause = 0;
  struct stat st;
  if (stat(buf,&st) != 0) {
    // Can't find config file in config dir-
    printf("INIT: Can't find configuration file '%s'.\n",buf);
    pause = 1;

    // Check if config dir exists?
    sprintf(buf,"%s/%s",homedir,FWEELIN_CONFIG_DIR);
    if (mkdir(buf,S_IRWXU)) {
      if (errno != EEXIST) {
	printf("INIT: Error %d creating config folder '%s'- "
	       "do you have write permission there?\n",
	       errno,buf);
      }
    } else 
      printf("INIT: *** Created new config folder '%s'.\n",buf);

    // Look in home dir (old placement)
    printf("INIT: Checking home folder (old place).\n");
    sprintf(buf,"mv %s/%s %s/%s",
	    homedir,FWEELIN_CONFIG_FILE,
	    homedir,FWEELIN_CONFIG_DIR);
    printf("INIT: Executing: %s\n",buf);
    system(buf);

    sprintf(buf,"%s/%s/%s",homedir,FWEELIN_CONFIG_DIR,FWEELIN_CONFIG_FILE);
    if (stat(buf,&st) != 0) {
      // Still can't find config file-
      printf("INIT: Checking shared folder (install place).\n");
      
      sprintf(buf,"cp %s/%s %s/%s",
	      FWEELIN_DATADIR,FWEELIN_CONFIG_FILE,
	      homedir,FWEELIN_CONFIG_DIR);
      printf("INIT: Executing: %s\n",buf);
      system(buf);
      
      sprintf(buf,"%s/%s/%s",homedir,FWEELIN_CONFIG_DIR,FWEELIN_CONFIG_FILE);
      if (stat(buf,&st) != 0) {
	printf("INIT: Can't find our configuration file '%s' anywhere!\n"
	       "Did you run 'make install'?\n",
	       FWEELIN_CONFIG_FILE);
	exit(1);
      } else
	printf("INIT: Config file installed @ '%s'.\n",buf);
    } else
      printf("INIT: Config file migrated to '%s'.\n",buf);
  }

  if (pause) {
    printf("*** Please review the above change "
	   "and press <ENTER> to continue.\n");
    getchar();
  }

  doc = xmlParseFile(buf);
  if (doc == 0) {
    printf("INIT: Error parsing config file.\n");
    exit(1);
  } else {
    xmlNode *root = NULL;
    xmlNode *cur_node;

    /*Get the root element node */
    root = xmlDocGetRootElement(doc);
    
    if (!root || !root->name ||
	xmlStrcmp(root->name,(const xmlChar *) "freewheeling") ) 
      printf("INIT: Config file format invalid-- bad XML root name\n");
    else {
      // Get config file version and make sure it matches our version
      xmlChar *ver = xmlGetProp(root, (const xmlChar *)"version");
      if (ver == 0 || strcmp((char *) ver,VERSION)) {
	printf("INIT: ERROR: Config file version \"%s\" does not match "
	       "FreeWheeling version \"%s\"!\n\n",ver,VERSION);
	printf("Would you like to override your old config with the new file\n"
	       "(warning: You'll lose any config changes you've made!) "
	       "(y/N)? ");
 	char c = getchar();

	if (c == 'Y' || c == 'y') {
	  char *homedir = getenv("HOME");
	  char buf[strlen(FWEELIN_DATADIR) + strlen(homedir) + 
		   strlen(FWEELIN_CONFIG_FILE) + 
		   strlen(FWEELIN_CONFIG_DIR) + 11];
	  sprintf(buf,"cp %s/%s %s/%s",FWEELIN_DATADIR,FWEELIN_CONFIG_FILE,
		  homedir,FWEELIN_CONFIG_DIR);
	  printf("\nINIT: Executing: %s\n",buf);
	  system(buf);

	  // Free and restart
	  printf("Reading new config...\n");
	  sprintf(buf,"%s/%s/%s",
		  homedir,FWEELIN_CONFIG_DIR,FWEELIN_CONFIG_FILE);
	  xmlFreeDoc(doc);
	  doc = xmlParseFile(buf);

	  /*Get the root element node */
	  root = xmlDocGetRootElement(doc);
	  xmlChar *ver = xmlGetProp(root, (const xmlChar *)"version");
	  if (ver == 0 || strcmp((char *) ver,VERSION)) {
	    printf("INIT: ERROR: Config in install dir is not up to date!\n"
		   "Did you run 'make install'?\n");
	    exit(1);
	  }
	}
	else {
	  printf("\nYou can manually edit your config file to match the "
		 "new one-- here is a diff of the two: \n\n");

	  char *homedir = getenv("HOME");
	  char buf[strlen(FWEELIN_DATADIR) + strlen(homedir) + 
		   2*strlen(FWEELIN_CONFIG_FILE) + 
		   strlen(FWEELIN_CONFIG_DIR) + 11];
	  sprintf(buf,"diff %s/%s %s/%s/%s",
		  FWEELIN_DATADIR,FWEELIN_CONFIG_FILE,
		  homedir,FWEELIN_CONFIG_DIR,FWEELIN_CONFIG_FILE);
	  printf("INIT: Executing: %s\n",buf);
	  system(buf);

	  exit(1);
	}
      } else
	xmlFree(ver);
      
      for (cur_node = root->children; cur_node != NULL; 
	   cur_node = cur_node->next) {
	if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"general"))) {
	  // General cfg
	  xmlNode *gen = cur_node;
	  ConfigureGeneral(doc,gen);
	} else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"video"))) {
	  // Video cfg
	  xmlNode *vid = cur_node;
	  ConfigureVideo(doc,vid);
	} else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"events"))) {
	  // Events
	  xmlNode *events = cur_node;
	  ConfigureEventBindings(doc,events);
	}
      }
    }
    
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
  }
};