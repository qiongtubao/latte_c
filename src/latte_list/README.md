


//java api
int size();
bool isEmpty();
bool contains(void* o);
latte_list_iter iterator();
void** toArray();
bool add(void* e);
bool remove(void* e);
bool containsAll(latte_list* c);
bool addAll(latte_list* list);
bool removeAll(latte_list* list);
bool retainAll(latte_list* list);
void replaceAll(latte_list* list);
void sort(latte_list* list);
void clear();
bool equals();
int hashCode();
void* get(int index);
void* set(int index, void* e);
void add(int index, void* e);
void remove(int index);
int indexOf(void* e);
int lastIndexOf(void* e);
latte_list_iter* listIterator(int index);
latte_list* subList(int start, int end);
