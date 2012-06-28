/* $Id$
 */

#ifndef _LINK_H
#define _LINK_H

#ifdef __cplusplus

typedef struct {
	OBJECT *obj;
	PROPERTY *prop;
} OBJECTPROPERTY;

typedef struct s_linklist {
	char *name; // spec for link
	void *data; // local data
	void *addr; // remote data
	size_t size; // size of data
	size_t index; // index to data
	struct s_linklist *next;
} LINKLIST;

class link {
private: // target data link
	char target[64]; ///< name of target
	LINKLIST *globals; ///< list of globals to publish  
	LINKLIST *objects; ///< list of objects to publish  
	LINKLIST *exports; ///< list of objects to export
	LINKLIST *imports; ///< list of objects to import  
	void *data;	///< other data associated with this link

private: // target function link
	void *handle;
	bool (*settag)(link *mod, char *,char*);
	bool (*init)(link *mod);
	TIMESTAMP (*sync)(link *mod, TIMESTAMP t0);
	bool (*term)(link *mod);

public:
	void *get_handle();
	inline bool do_init() { return init==NULL ? false : (*init)(this); };
	inline TIMESTAMP do_sync(TIMESTAMP t) { return sync==NULL ? TS_NEVER : (*sync)(this,t); };
	inline bool do_term(void) { return term==NULL ? true : (*term)(this); };

private: // link list
	class link *next; ///< pointer to next link target
	static class link *first; ///< pointer for link target

public: // link list accessors
	inline static class link *get_first() { return first; }
	inline class link *get_next() { return next; };

public: // construction/destruction
	link(char *file);
	~link(void);

public: // accessors
	bool set_target(char *data);
	inline char *get_target(void) { return target; };
	LINKLIST *add_global(char *name);
	LINKLIST *add_object(char *name);
	LINKLIST *add_export(char *name);
	LINKLIST *add_import(char *name);
	inline void set_data(void *ptr) { data=ptr; };
	void *get_data(void) { return data; };

	// linklist accessors
	inline char *get_name(LINKLIST *item) { return (char*)item->name; }; 
	inline LINKLIST *get_next(LINKLIST *item) { return item->next; };
	inline void *get_data(LINKLIST *item) { return item->data; };
	inline void set_data(LINKLIST *item, void *ptr) { item->data=ptr; };
	inline void set_addr(LINKLIST *item,void*ptr) { item->addr = ptr; };
	inline void *get_addr(LINKLIST *item) { return item->addr; };
	inline void set_size(LINKLIST *item, size_t size) { item->size = size; };
	inline size_t get_size(LINKLIST *item) { return item->size; };
	inline void set_index(LINKLIST *item, size_t index) { item->index = index; };
	inline size_t get_index(LINKLIST *item) { return item->index; };

	inline LINKLIST *get_globals(void) { return globals; };
	inline GLOBALVAR *get_globalvar(LINKLIST *item) { return (GLOBALVAR*)item->data; };
	inline void set_globalvar(LINKLIST *item, GLOBALVAR *var) { item->data=(void*)var; };

	inline LINKLIST *get_objects(void) { return objects; };
	inline OBJECT *get_object(LINKLIST *item) { return (OBJECT*)item->data; };
	inline void set_object(LINKLIST *item, OBJECT *var) { item->data=(void*)var; };

	inline LINKLIST *get_imports(void) { return imports; };
	inline OBJECTPROPERTY *get_import(LINKLIST *item) { return (OBJECTPROPERTY*)item->data; };
	inline void set_import(LINKLIST *item, OBJECT *obj, PROPERTY *prop) 
	{
		OBJECTPROPERTY *var = (OBJECTPROPERTY*)malloc(sizeof(OBJECTPROPERTY));
		var->obj = obj;
		var->prop = prop;
		if ( item->data!=NULL ) free(item->data);
		item->data=(void*)var; 
	};
	
	inline LINKLIST *get_exports(void) { return exports; };
	inline OBJECTPROPERTY *get_export(LINKLIST *item) { return (OBJECTPROPERTY*)item->data; };
	inline void set_export(LINKLIST *item, OBJECT *obj, PROPERTY *prop) 
	{
		OBJECTPROPERTY *var = (OBJECTPROPERTY*)malloc(sizeof(OBJECTPROPERTY));
		var->obj = obj;
		var->prop = prop;
		if ( item->data!=NULL ) free(item->data);
		item->data=(void*)var; 
	};
	
};

extern "C" {
#endif

int link_create(char *name);
int link_initall(void);
TIMESTAMP link_syncall(TIMESTAMP t0);
int link_termall();

#ifdef __cplusplus
}
#endif

#endif