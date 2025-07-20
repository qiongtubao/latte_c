#include "avl_tree.h"
#include "zmalloc/zmalloc.h"



void avl_node_recursively(avl_node_t** node, avl_tree_type_t* type) {
    if (*node != NULL) {
        avl_node_recursively(&(*node)->left, type);
        avl_node_recursively(&(*node)->right, type);
        type->release_node(*node);
        *node = NULL;
    }
}

void avl_tree_delete(avl_tree_t* tree) {
    avl_node_recursively(&tree->root, tree->type);
    zfree(tree);
}

void avl_tree_clear(avl_tree_t* tree) {
    avl_node_recursively(&tree->root, tree->type);
}

int avl_node_get_height(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return node->height;
}
#define max(a,b) a < b? b: a

int get_balance(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return avl_node_get_height(node->left) - avl_node_get_height(node->right);
}

avl_node_t* rotate_right(avl_node_t* y) {
    avl_node_t* x = y->left;
    avl_node_t* T2 = x->right;

    // Perform rotation
    x->right = y;
    y->left = T2;

    // Update heights
    y->height = max(avl_node_get_height(y->left), avl_node_get_height(y->right)) + 1;
    x->height = max(avl_node_get_height(x->left), avl_node_get_height(x->right)) + 1;

    return x;
}

avl_node_t* rotate_left(avl_node_t* x) {
    avl_node_t* y = x->right;
    avl_node_t* T2 = y->left;

    // Perform rotation
    y->left = x;
    x->right = T2;

    // Update heights
    x->height = max(avl_node_get_height(x->left), avl_node_get_height(x->right)) + 1;
    y->height = max(avl_node_get_height(y->left), avl_node_get_height(y->right)) + 1;

    return y;
}

avl_node_t* avl_node_insert(avl_node_t* node, avl_tree_type_t* type, void* key, void* value, int* puted) {
    if (node == NULL) {
        avl_node_t* node = type->create_node(key, value);
        node->left = node->right = NULL;
        node->height = 1;
        *puted = 1;
        return node;
    }
    int cmp = type->operator(key, node->key);
    if (cmp < 0) {
        node->left = avl_node_insert(node->left, type, key, value, puted);
    } else if (cmp > 0) {
        node->right = avl_node_insert(node->right, type, key, value, puted);
    } else {
        type->node_set_val(node, value);
        *puted = 0;
    }

    node->height = 1 + max(avl_node_get_height(node->left), avl_node_get_height(node->right));

    int balance = get_balance(node);
    if (balance > 1 && type->operator(key, node->left->key) > 0) {
        return rotate_right(node);
    }

    if (balance < -1 && type->operator(key, node->right->key) < 0) {
        return rotate_left(node);
    }

    if (balance > 1 && type->operator(key, node->left->key) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    if (balance < -1 && type->operator(key, node->right->key) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
} 

/**
 *  -1 添加失败
 *  0  更新
 *  1  添加
 */
int avl_tree_put(avl_tree_t* tree, void* key, void* value) {
    int puted = -1;
    tree->root = avl_node_insert(tree->root, tree->type, key, value, &puted);
    return puted;
}

void* avl_node_search(avl_node_t* node, avl_tree_type_t* type, void* key) {
    if (node == NULL) {
        return NULL;
    }
    int result = type->operator(node->key,  key);
    if (result == 0) return node;
    if (result < 0) {
        return avl_node_search(node->left, type, key);
    }
    return avl_node_search(node->right, type, key);
}

void* avl_tree_get(avl_tree_t* tree, void* key) {
    return avl_node_search(tree->root, tree->type, key);
}

avl_node_t* find_min_value_node(avl_node_t* node) {
    avl_node_t* current = node;
    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

avl_node_t* avl_tree_get_min(avl_tree_t* tree) {
    return find_min_value_node(tree->root);
}

avl_node_t* delete_child_node(avl_node_t* node, avl_tree_type_t* type, void* key, int* deleted) {
    if (node == NULL) return node;

    int result = type->operator(key, node->key);
    if (result > 0) {
        node->left = delete_child_node(node->left, type, key, deleted);
    } else if (result < 0) {
        node->right = delete_child_node(node->right, type, key, deleted);
    } else {
        if ((node ->left == NULL)  || (node->right == NULL)) {
            avl_node_t* temp = node->left ? node->left: node->right;
            if (temp == NULL) {
                temp = node;
                node = NULL;
            } else {
                *node = *temp;
            }
            *deleted = 1;
            type->release_node(temp);
        } else {
            avl_node_t* temp = find_min_value_node(node->right);
            node->key = temp->key;
            node->right = delete_child_node(node->right, type, temp->key, deleted);
        }
    }

    if (node == NULL) return node;

    node->height = 1 + max(avl_node_get_height(node->left), avl_node_get_height(node->right));

    int balance = get_balance(node);

    if (balance > 1 && get_balance(node->left) >= 0) {
        return rotate_right(node);
    }

    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    if (balance < -1 && get_balance(node->right) <= 0) {
        return rotate_left(node);
    }

    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
}

int avl_tree_remove(avl_tree_t* tree, void* key) {
    int delete = 0;
    tree->root = delete_child_node(tree->root, tree->type, key, &delete);
    return delete;
}



int node_length(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return 1 + node_length(node->left) + node_length(node->right);
}

int avl_tree_size(avl_tree_t* tree) {
    return node_length(tree->root);
}


avl_tree_iterator_t* avl_tree_iterator_new(avl_tree_t* tree) {
    avl_tree_iterator_t* it = zmalloc(sizeof(avl_tree_iterator_t));
    it->current = tree->root;
    it->stackSize = 16; //初始栈大小
    it->stack = zmalloc(it->stackSize * sizeof(avl_node_t*));
    it->top = 0;

    //将所有左孩子入栈
    while (it->current != NULL) {
        it->stack[it->top++] = it->current;
        if (it->top == it->stackSize) {
            it->stackSize *= 2;
            it->stack = zrealloc(it->stack, it->stackSize * sizeof(avl_node_t*));
        }
        it->current = it->current->left;
    }
    return it;
}



void* avl_tree_iterator_next(latte_iterator_t* it_) {
    avl_tree_iterator_t* data = it_->data;
    if (data->top <= 0) {
        return NULL;
    }
    avl_node_t* node = data->stack[--data->top];
    data->current = node->right;
    while(data->current != NULL) {
        data->stack[data->top++] = data->current;
        if (data->top == data->stackSize) {
            data->stackSize *= 2;
            data->stack = zrealloc(data->stack, data->stackSize * sizeof(avl_node_t*));
        }
        data->current = data->current->left;
    }
    avl_tree_latte_iterator_t* iterator = (avl_tree_latte_iterator_t*)it_;
    iterator->pair.key = node->key;
    iterator->pair.value =  iterator->tree->type->get_value == NULL ? NULL: iterator->tree->type->get_value(node);
    return &iterator->pair;
}

void avl_tree_iterator_delete(latte_iterator_t* iterator) {
    avl_tree_iterator_t* data = iterator->data;
    zfree(data->stack);
    zfree(data);
    zfree(iterator);
}

bool avl_tree_iterator_has_next(latte_iterator_t* iterator) {
    avl_tree_iterator_t* data = iterator->data;
    return data->top > 0;
}

latte_iterator_func avl_tree_iterator_tType = {
     .next = avl_tree_iterator_next,
     .release = avl_tree_iterator_delete,
     .has_next = avl_tree_iterator_has_next
};
latte_iterator_t* avl_tree_get_iterator(avl_tree_t* tree) {
    avl_tree_latte_iterator_t* iterator = zmalloc(sizeof(avl_tree_latte_iterator_t));
    iterator->iterator.func = &avl_tree_iterator_tType;
    iterator->iterator.data = avl_tree_iterator_new(tree);
    iterator->tree  = tree;
    iterator->pair.key = NULL;
    iterator->pair.value = NULL;
    return (latte_iterator_t*)iterator;
}

avl_tree_t* avl_tree_new(avl_tree_type_t* type) {
    avl_tree_t* tree = zmalloc(sizeof(avl_tree_t));
    tree->type = type;
    tree->root = NULL;
    return tree;
}