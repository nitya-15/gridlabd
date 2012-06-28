// $Id: matlab.cpp
// Copyright (C) 2012 Battelle Memorial Institute

#include <stdlib.h>
#include <ctype.h>

// you must have matlab installed and ensure matlab/extern/include is in include path
#include <matrix.h>
#include <engine.h>

// you must have gridlabd installed and ensure core is in include path
#include <gridlabd.h>
#include <link.h>

CALLBACKS *callback = NULL;

typedef enum {
	MWD_HIDE, // never show window
	MWD_SHOW, // show only while running
	MWD_KEEP, // show while running and keep open when done
	MWD_ONERROR, // show only on error and keep open when done
	MWD_ONDEBUG, // show only when debugging and keep open when done
} MATLABWINDOWDISPOSITION;

typedef struct {
	Engine *engine;
	MATLABWINDOWDISPOSITION window;
	int keep_onerror;
	char *init;
	char *sync;
	char *term;
	int status;
	char rootname[64];
	mxArray *root;
} MATLABLINK;

static mxArray* create_mxproperty(gld_property *prop)
{
	mxArray *value=NULL;
	switch ( prop->get_type() ) {
	case PT_double:
	case PT_random:
	case PT_enduse:
	case PT_loadshape:
		value = mxCreateDoubleScalar(*(double*)prop->get_addr());
		break;
	case PT_complex:
		{
			value = mxCreateDoubleMatrix(1,1,mxCOMPLEX);
			complex *v = (complex*)prop->get_addr();
			*mxGetPr(value) = v->Re();
			*mxGetPi(value) = v->Im();
		}
		break;
	case PT_int16:
		value = mxCreateDoubleScalar((double)*(int16*)prop->get_addr());
		break;
	case PT_enumeration:
	case PT_int32:
		value = mxCreateDoubleScalar((double)*(int32*)prop->get_addr());
		break;
	case PT_set:
	case PT_int64:
		value = mxCreateDoubleScalar((double)*(int64*)prop->get_addr());
		break;
	case PT_timestamp:
		value = mxCreateDoubleScalar((double)*(TIMESTAMP*)prop->get_addr());
		break;
	case PT_bool:
		value = mxCreateDoubleScalar((double)*(bool*)prop->get_addr());
		break;
	case PT_char8:
	case PT_char32:
	case PT_char256:
	case PT_char1024:
		{
			const char *str[] = {(char*)prop->get_addr()};
			value = mxCreateCharMatrixFromStrings(mwSize(1),str); 
		}
		break;
	default:
		value = NULL;
		break;
	}
	return value;
}
static mxArray* set_mxproperty(mxArray *value, gld_property *prop)
{
	switch ( prop->get_type() ) {
	case PT_double:
	case PT_random:
	case PT_enduse:
	case PT_loadshape:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = *(double*)prop->get_addr();
		}
		break;
	case PT_complex:
		{
			double *r = mxGetPr(value);
			double *i = mxGetPi(value);
			complex *v = (complex*)prop->get_addr();
			if (r) *r = v->Re();
			if (i) *i = v->Im();
		}
		break;
	case PT_int16:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = (double)*(int16*)prop->get_addr();
		}
		break;
	case PT_enumeration:
	case PT_int32:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = (double)*(int32*)prop->get_addr();
		}
		break;
	case PT_set:
	case PT_int64:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = (double)*(int64*)prop->get_addr();
		}
		break;
	case PT_timestamp:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = (double)*(TIMESTAMP*)prop->get_addr();
		}
		break;
	case PT_bool:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *ptr = (double)*(bool*)prop->get_addr();
		}
		break;
	case PT_char8:
	case PT_char32:
	case PT_char256:
	case PT_char1024:
		// TODO
		break;
	default:
		value = NULL;
		break;
	}
	return value;
}
static mxArray* get_mxproperty(mxArray *value, gld_property *prop)
{
	switch ( prop->get_type() ) {
	case PT_double:
	case PT_random:
	case PT_enduse:
	case PT_loadshape:
		{
			double *ptr = mxGetPr(value);
			*(double*)prop->get_addr() = *ptr;
		}
		break;
	case PT_complex:
		{
			double *r = mxGetPr(value);
			double *i = mxGetPi(value);
			complex *v = (complex*)prop->get_addr();
			if (r) v->Re() = *r;
			if (i) v->Im() = *i;
		}
		break;
	case PT_int16:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *(int16*)prop->get_addr() = (int16)*ptr;
		}
		break;
	case PT_enumeration:
	case PT_int32:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *(int32*)prop->get_addr() = (int32)*ptr;
		}
		break;
	case PT_set:
	case PT_int64:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *(int64*)prop->get_addr() = (int64)*ptr;
		}
		break;
	case PT_timestamp:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *(TIMESTAMP*)prop->get_addr() = (TIMESTAMP)*ptr;
		}
		break;
	case PT_bool:
		{
			double *ptr = mxGetPr(value);
			if (ptr) *(bool*)prop->get_addr() = (bool)*ptr;
		}
		break;
	case PT_char8:
	case PT_char32:
	case PT_char256:
	case PT_char1024:
		// TODO
		break;
	default:
		value = NULL;
		break;
	}
	return value;
}
EXPORT bool create(link *mod, CALLBACKS *fntable)
{
	callback = fntable;
	MATLABLINK *matlab = new MATLABLINK;
	memset(matlab,0,sizeof(MATLABLINK));
	strcpy(matlab->rootname,"gridlabd");
	mod->set_data(matlab);
	return true;
}

EXPORT bool settag(link *mod, char *tag, char *data)
{
	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();
	if ( strcmp(tag,"window")==0 )
	{
		if ( strcmp(data,"show")==0 )
			matlab->window = MWD_SHOW;
		else if ( strcmp(data,"hide")==0 )
			matlab->window = MWD_HIDE;
		else if ( strcmp(data,"onerror")==0 )
			matlab->window = MWD_ONERROR;
		else if ( strcmp(data,"ondebug")==0 )
			matlab->window = MWD_ONDEBUG;
		else if ( strcmp(data,"keep")==0 )
			matlab->window = MWD_KEEP;
		else
			gl_error("'%s' is not a valid matlab window disposition", data);
	}
	else if ( strcmp(tag,"root")==0 )
	{
		if ( strlen(data)<sizeof(matlab->rootname) )
			strcpy(matlab->rootname,data);
		else
			gl_error("root name is too long (max is %d)", sizeof(matlab->rootname));
	}
	else if ( strcmp(tag,"on_init")==0 )
	{
		size_t len = strlen(data);
		if ( len>0 )
		{
			matlab->init = (char*)malloc(len+1);
			strcpy(matlab->init,data);
		}
	}
	else if ( strcmp(tag,"on_sync")==0 )
	{
		size_t len = strlen(data);
		if ( len>0 )
		{
			matlab->sync = (char*)malloc(len+1);
			strcpy(matlab->sync,data);
		}
	}
	else if ( strcmp(tag,"on_term")==0 )
	{
		size_t len = strlen(data);
		if ( len>0 )
		{
			matlab->term = (char*)malloc(len+1);
			strcpy(matlab->term,data);
		}
	}
	else
	{
		gl_output("tag '%s' not valid for matlab target", tag);
		return false;
	}
	return true;
}

bool window_show(MATLABLINK *matlab)
{
	// return true if window should be visible
	switch ( matlab->window ) {
	case MWD_HIDE: return false;
	case MWD_SHOW: return true;
	case MWD_KEEP: return true;
	case MWD_ONERROR: return true;
	case MWD_ONDEBUG: return true; // TODO read global debug variable
	default: return false;
	}
}
bool window_kill(MATLABLINK *matlab)
{
	// return true if window engine should be shutdown
	switch ( matlab->window ) {
	case MWD_HIDE: return true;
	case MWD_SHOW: return true;
	case MWD_KEEP: return false;
	case MWD_ONERROR: return false; // TODO read exit status
	case MWD_ONDEBUG: return true; // TODO read global debug variable
	default: return true;
	}
}

EXPORT bool init(link *mod)
{
	gl_verbose("initialization matlab link");

	// initialize matlab engine
	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();
	matlab->engine = engOpenSingleUse(NULL,NULL,&matlab->status);
	if ( matlab->engine==NULL )
		return false;

	gl_debug("matlab link is open");

	// setup matlab engine
	engSetVisible(matlab->engine,window_show(matlab));
	if ( matlab->init ) engEvalString(matlab->engine,matlab->init);

	// build gridlabd data
	mwSize dims[] = {1,1};
	mxArray *gridlabd_struct = mxCreateStructArray(2,dims,0,NULL);

	///////////////////////////////////////////////////////////////////////////
	// build global data
	LINKLIST *item;
	mxArray *global_struct = mxCreateStructArray(2,dims,0,NULL);
	for ( item=mod->get_globals() ; item!=NULL ; item=mod->get_next(item) )
	{
		char *name = mod->get_name(item);
		GLOBALVAR *var = mod->get_globalvar(item);
		mxArray *var_struct = NULL;
		mwIndex var_index;
		if ( var==NULL ) continue;

		// do not map module or structured globals
		if ( strchr(var->prop->name,':')!=NULL )
		{
			// ignore module globals here
		}
		else if ( strchr(var->prop->name,'.')!=NULL )
		{
			char struct_name[256];
			if ( sscanf(var->prop->name,"%[^.]",struct_name)==0 )
			{
				gld_property prop(var);
				var_index = mxAddField(global_struct,prop.get_name());
				var_struct = create_mxproperty(&prop);
				if ( var_struct!=NULL )
				{
					//mod->add_copyto(var->prop->addr,mxGetData(var_struct));
					mxSetFieldByNumber(global_struct,0,var_index,var_struct);
				}
			}
		}
		else // simple data
		{
			gld_property prop(var);
			var_index = mxAddField(global_struct,prop.get_name());
			var_struct = create_mxproperty(&prop);
			if ( var_struct!=NULL )
			{
				//mod->add_copyto(var->prop->addr,mxGetData(var_struct));
				mxSetFieldByNumber(global_struct,0,var_index,var_struct);
			}
		}

		// update export list
		if ( var_struct!=NULL )
		{
			mod->set_addr(item,(void*)var_struct);
			mod->set_index(item,(size_t)var_index);
		}
	}

	// add globals structure to gridlabd structure
	mwIndex gridlabd_index = mxAddField(gridlabd_struct,"global");
	mxSetFieldByNumber(gridlabd_struct,0,gridlabd_index,global_struct);

	///////////////////////////////////////////////////////////////////////////
	// build module data
	dims[0] = dims[1] = 1;
	mxArray *module_struct = mxCreateStructArray(2,dims,0,NULL);

	// add modules
	for ( MODULE *module = callback->module.getfirst() ; module!=NULL ; module=module->next )
	{
		// create module info struct
		mwIndex dims[] = {1,1};
		mxArray *module_data = mxCreateStructArray(2,dims,0,NULL);
		mwIndex module_index = mxAddField(module_struct,module->name);
		mxSetFieldByNumber(module_struct,0,module_index,module_data);
		
		// create version info struct
		const char *version_fields[] = {"major","minor"};
		mxArray *version_data = mxCreateStructArray(2,dims,sizeof(version_fields)/sizeof(version_fields[0]),version_fields);
		mxArray *major_data = mxCreateDoubleScalar((double)module->major);
		mxArray *minor_data = mxCreateDoubleScalar((double)module->minor);
		mxSetFieldByNumber(version_data,0,0,major_data);
		mxSetFieldByNumber(version_data,0,1,minor_data);

		// attach version info to module info
		mwIndex version_index = mxAddField(module_data,"version");
		mxSetFieldByNumber(module_data,0,version_index,version_data);

	}
	gridlabd_index = mxAddField(gridlabd_struct,"module");
	mxSetFieldByNumber(gridlabd_struct,0,gridlabd_index,module_struct);

	///////////////////////////////////////////////////////////////////////////
	// build class data
	dims[0] = dims[1] = 1;
	mxArray *class_struct = mxCreateStructArray(2,dims,0,NULL);
	gridlabd_index = mxAddField(gridlabd_struct,"class");
	mxSetFieldByNumber(gridlabd_struct,0,gridlabd_index,class_struct);
	mwIndex class_id[1024]; // index into class struct
	memset(class_id,0,sizeof(class_id));

	// add classes
	for ( CLASS *oclass = callback->class_getfirst() ; oclass!=NULL ; oclass=oclass->next )
	{
		// count objects in this class
		mwIndex dims[] = {0,1};
		for ( item=mod->get_exports() ; item!=NULL ; item=mod->get_next(item) )
		{
			OBJECT *obj = mod->get_object(item);
			if ( obj==NULL || obj->oclass!=oclass ) continue;
			dims[0]++;
		}
		if ( dims[0]==0 ) continue;
		mxArray *runtime_struct = mxCreateStructArray(2,dims,0,NULL);

		// add class 
		mwIndex class_index = mxAddField(class_struct,oclass->name);
		mxSetFieldByNumber(class_struct,0,class_index,runtime_struct);

		// add properties to class
		for ( PROPERTY *prop=oclass->pmap ; prop!=NULL && prop->oclass==oclass ; prop=prop->next )
		{
			mwIndex dims[] = {1,1};
			mxArray *property_struct = mxCreateStructArray(2,dims,0,NULL);
			mwIndex runtime_index = mxAddField(runtime_struct,prop->name);
			mxSetFieldByNumber(runtime_struct,0,runtime_index,property_struct);
		}

		// add objects to class
		for ( item=mod->get_exports() ; item!=NULL ; item=mod->get_next(item) )
		{
			OBJECT *obj = mod->get_object(item);
			if ( obj==NULL || obj->oclass!=oclass ) continue;
			mwIndex index = class_id[obj->oclass->id]++;
			
			// add properties to class
			for ( PROPERTY *prop=oclass->pmap ; prop!=NULL && prop->oclass==oclass ; prop=prop->next )
			{
				gld_property p(obj,prop);
				mxArray *data = create_mxproperty(&p);
				mxSetField(runtime_struct,index,prop->name,data);
			}

			// update export list
			mod->set_addr(item,(void*)runtime_struct);
			mod->set_index(item,(size_t)index);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// build the object data
	dims[0] = 0;
	for ( item=mod->get_exports() ; item!=NULL ; item=mod->get_next(item) )
	{
		if ( mod->get_object(item)!=NULL ) dims[0]++;
	}
	dims[1] = 1;
	memset(class_id,0,sizeof(class_id));
	const char *objfields[] = {"name","class","id","parent","rank","clock","valid_to","schedule_skew",
		"latitude","longitude","in","out","rng_state","heartbeat","lock","flags"};
	mxArray *object_struct = mxCreateStructArray(2,dims,sizeof(objfields)/sizeof(objfields[0]),objfields);
	mwIndex n=0;
	for ( item=mod->get_exports() ; item!=NULL ; item=mod->get_next(item) )
	{
		OBJECT *obj = mod->get_object(item);
		if ( obj==NULL ) continue;
		class_id[obj->oclass->id]++; // index into class struct

		const char *objname[] = {obj->name&&isdigit(obj->name[0])?NULL:obj->name};
		const char *oclassname[] = {obj->oclass->name};

		if (obj->name) mxSetFieldByNumber(object_struct,n,0,mxCreateCharMatrixFromStrings(mwSize(1),objname));
		mxSetFieldByNumber(object_struct,n,1,mxCreateCharMatrixFromStrings(mwSize(1),oclassname));
		mxSetFieldByNumber(object_struct,n,2,mxCreateDoubleScalar((double)class_id[obj->oclass->id]));
		if (obj->parent) mxSetFieldByNumber(object_struct,n,3,mxCreateDoubleScalar((double)obj->parent->id+1));
		mxSetFieldByNumber(object_struct,n,4,mxCreateDoubleScalar((double)obj->rank));
		mxSetFieldByNumber(object_struct,n,5,mxCreateDoubleScalar((double)obj->clock));
		mxSetFieldByNumber(object_struct,n,6,mxCreateDoubleScalar((double)obj->valid_to));
		mxSetFieldByNumber(object_struct,n,7,mxCreateDoubleScalar((double)obj->schedule_skew));
		if ( isfinite(obj->latitude) ) mxSetFieldByNumber(object_struct,n,8,mxCreateDoubleScalar((double)obj->latitude));
		if ( isfinite(obj->longitude) ) mxSetFieldByNumber(object_struct,n,9,mxCreateDoubleScalar((double)obj->longitude));
		mxSetFieldByNumber(object_struct,n,10,mxCreateDoubleScalar((double)obj->in_svc));
		mxSetFieldByNumber(object_struct,n,11,mxCreateDoubleScalar((double)obj->out_svc));
		mxSetFieldByNumber(object_struct,n,12,mxCreateDoubleScalar((double)obj->rng_state));
		mxSetFieldByNumber(object_struct,n,13,mxCreateDoubleScalar((double)obj->heartbeat));
		mxSetFieldByNumber(object_struct,n,14,mxCreateDoubleScalar((double)obj->lock));
		mxSetFieldByNumber(object_struct,n,15,mxCreateDoubleScalar((double)obj->flags));
		n++;
	}
	gridlabd_index = mxAddField(gridlabd_struct,"object");
	mxSetFieldByNumber(gridlabd_struct,0,gridlabd_index,object_struct);

	///////////////////////////////////////////////////////////////////////////
	// post the gridlabd structure
	matlab->root = gridlabd_struct;
	engPutVariable(matlab->engine,matlab->rootname,matlab->root);

	return true;
}

bool copy_exports(link *mod)
{
	return true;

	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();
	LINKLIST *item;

	// update globals
	for ( item=mod->get_globals() ; item!=NULL ; item=mod->get_next(item) )
	{
		mxArray *var_struct = (mxArray*)mod->get_addr(item);
		if ( var_struct!=NULL )
		{
			mwIndex var_index = (mwIndex)mod->get_index(item);
			GLOBALVAR *var = mod->get_globalvar(item);
			gld_property prop(var);
			set_mxproperty(var_struct,&prop);
		}
	}

	// update exports
	for ( item=mod->get_exports() ; item!=NULL ; item=mod->get_next(item) )
	{
		OBJECT *obj = mod->get_object(item);
		if ( obj==NULL ) continue;
		mwIndex index = mod->get_index(item);
		mxArray *runtime_struct = (mxArray*)mod->get_addr(item); 
		
		// add properties to class
		CLASS *oclass = obj->oclass;
		for ( PROPERTY *prop=oclass->pmap ; prop!=NULL && prop->oclass==oclass ; prop=prop->next )
		{
			gld_property p(obj,prop);
			mxArray *data = mxGetField(runtime_struct,index,prop->name);
			set_mxproperty(data,&p);
		}
	}

	engPutVariable(matlab->engine,matlab->rootname,matlab->root);
	return true;
}

bool copy_imports(link *mod)
{
	return true;

	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();
	mxDestroyArray(matlab->root);
	matlab->root = engGetVariable(matlab->engine,matlab->rootname);
	LINKLIST *item;

	// update globals
	for ( item=mod->get_globals() ; item!=NULL ; item=mod->get_next(item) )
	{
		mxArray *var_struct = (mxArray*)mod->get_addr(item);
		if ( var_struct!=NULL )
		{
			mwIndex var_index = (mwIndex)mod->get_index(item);
			GLOBALVAR *var = mod->get_globalvar(item);
			gld_property prop(var);
			get_mxproperty(var_struct,&prop);
		}
	}

	// update imports
	for ( item=mod->get_imports()==NULL?mod->get_exports():mod->get_imports() ; item!=NULL ; item=mod->get_next(item) )
	{
		OBJECT *obj = mod->get_object(item);
		if ( obj==NULL ) continue;
		mwIndex index = mod->get_index(item);
		mxArray *runtime_struct = (mxArray*)mod->get_addr(item); 
		
		// add properties to class
		CLASS *oclass = obj->oclass;
		for ( PROPERTY *prop=oclass->pmap ; prop!=NULL && prop->oclass==oclass ; prop=prop->next )
		{
			gld_property p(obj,prop);
			mxArray *data = mxGetField(runtime_struct,index,prop->name);
			get_mxproperty(data,&p);
		}
	}

	return true;
}

EXPORT TIMESTAMP sync(link* mod,TIMESTAMP t0)
{
	TIMESTAMP t1 = TS_NEVER;
	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();

	if ( !copy_exports(mod) )
		return TS_INVALID; // error

	if ( matlab->sync ) engEvalString(matlab->engine,matlab->sync);
	mxArray *ans = engGetVariable(matlab->engine,"ans");
	if ( ans && mxIsDouble(ans) )
	{
		double *pVal = (double*)mxGetData(ans);
		if ( pVal!=NULL ) t1 = floor(*pVal);
	}

	if ( !copy_imports(mod) )
		return TS_INVALID; // error

	return TS_NEVER;
}

EXPORT bool term(link* mod)
{
	// close matlab engine
	MATLABLINK *matlab = (MATLABLINK*)mod->get_data();
	if ( matlab->term ) engEvalString(matlab->engine,matlab->term);
	if ( window_kill(matlab) ) engClose(matlab->engine)==0;
	return true;
}