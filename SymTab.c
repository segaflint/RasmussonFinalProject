/*
Code by: Seth Rasmusson
Last Modiefied: 2/1/2020

API for a symbol table. The symbol table stores (name, attribute) pairs. The data
type for the attribute is void * so programs that use the symbol table can
associate any attribute type with a name
  The symbol table is implemented using a separate chaining hash table.

*/
#include "SymTab.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int hashName(char *name, int table_size);
void free_SymEntry(SymEntry * entry);

SymTab * createSymTab(int size){
    SymTab * symTab_ret = (SymTab *) malloc(sizeof(SymTab));
    symTab_ret->contents = (SymEntry **) malloc(sizeof(SymEntry*) * size);
    symTab_ret->current = NULL;
    symTab_ret->size = size;
    int i;
    for(i = 0; i < size; i++) {
        symTab_ret->contents[i] = NULL;
    }

    /* PRE: size >= 0
    size is an estimate of the number of items that will be stored in the symbol
    table
    Return a pointer to a new symbol table
    */
    return symTab_ret;
}

//In the following functions assume a pre condition that table references a
//previously created symbol table
void destroySymTab(SymTab *table) {
    //recover space created by the symbol table functions
    //no functions should use the symbol table after it is destroyed

    //free all SymEntries
    int i;
    for(int i = 0; i < table->size; i++) {
        free_SymEntry(table->contents[i]);
    }

    //free attributes of table_size and the table itself
    free(table->contents);
    free(table->current);
    free(table);
}

int enterName(SymTab * table, char *name){
    /*if name is not in the symbol table, a copy of name is added to the symbol table
    with a NULL attribute, set current to reference the new (name, attribute) pair
    and return 1
    if name is in the symbol table, set current to reference the (name, attribute)
    pair and return 0
    */
    char * entry_name;
    int hashed_name;
    int found_name;
    SymEntry * local_current;

    hashed_name = hashName(name, table->size);
    local_current = table->contents[hashed_name];
    found_name = findName(table, name);

    if(found_name == 1) { // found and current set to whichever is found
        return 0;
    }

    //name not found; create a new entry and return 1
    entry_name = strdup(name);

    SymEntry * new_entry = (SymEntry *) malloc(sizeof(SymEntry));
    new_entry->name = entry_name;
    new_entry->attribute = NULL;
    new_entry->next = table->contents[hashed_name];
    table->contents[hashed_name] = new_entry;
    table->current = new_entry;

    return 1;
}

int findName(SymTab *table, char *name){
    /*if name is in the symbol table, set current to reference the (name, attribute)
    pair and return 1
    otherwise do not change current and return 0
    */
    int index = hashName(name, table->size);

    SymEntry * local_current = table->contents[index];
    while(local_current != NULL) {
        if( strcmp(name, local_current->name) == 0){
            table->current = local_current;
            return 1;
        }
        local_current = local_current->next;
    }
    return 0;
}


int hasCurrent(SymTab *table){
    //if current references a (name, attribute) pair return 1
    //otherwise return 0;
    if(table->current != NULL) return 1;
    else return 0;

}

//PRE: hasCurrent() == 1
void setCurrentAttr(SymTab *table, void * attr){
    //change the attribute value of the current (name, attribute) pair to attr
    table->current->attribute = attr;
}

//PRE: hasCurrent() == 1
void * getCurrentAttr(SymTab *table){
    //return the attribute in the current (name, attribute) pair
    return table->current->attribute;
}

//PRE: hasCurrent() == 1
char * getCurrentName(SymTab *table) {
    //return the name in the current (name, attribute) pair
    return table->current->name;
}

//Assume no changes are made to the symbol table should while iterating through the symbol table
int startIterator(SymTab *table) {
    //if the symbol table is empty, return 0
    //otherwise set current to the "first" (name, attribute) pair in the symbol table and return 1
    int i;
    for(i = 0; i < table->size; i++) {
        if(table->contents[i] != NULL) {
            table->current = table->contents[i];
            return 1;
        }
    }
    return 0;
}

int nextEntry(SymTab *table) {
    /*if all (name, attribute) pairs have been visited since the last call to
    startIterator, return 0
    otherwise set current to the "next" (name, attribute) pair and return 1
    */
    if(table->current->next != NULL){ //next entry is in the same list
        table->current = table->current->next;
        return 1;
    } else { // next entry in another index or none exists
        //find index of current entry,
        if(hashName(table->current->name, table->size) == table->size - 1) { //if at the end of the list
            return 0;
        } else { // else, not at the end of the list, try next index
            int i;
            for(i = hashName(table->current->name, table->size)+1; i < table->size; i++) {
                if(table->contents[i] != NULL) {
                    table->current = table->contents[i];
                    return 1;
                }
            }
            return 0;
        }
    }
}

/*
 * Return the integer corresponding to the hashed function based on the name given
 */
int hashName(char *name, int table_size) {
    int ret_val = 0;
    int i;
    for( i = 0; name[i] != '\0'; i++ ) {
        ret_val = ret_val + name[i];
    }
    ret_val = (ret_val % table_size);
    return ret_val;
}

/*
 * RECURSIVE
 * Free all space allocated to a SymEntry and its attributes,
 * including linked SymEntries recursively
 */
void free_SymEntry(SymEntry * entry) {
    if(entry == NULL) {
        free(entry);
        return;
    }
    free(entry->name);
    if(entry->next != NULL) {
        free_SymEntry(entry->next);
    }
}
