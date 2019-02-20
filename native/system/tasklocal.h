typedef struct TaskLocal_ TaskLocal;

struct TaskLocal_ {
    ChiDestructor destructor;
    TaskLocal*    next;
    uint8_t       data[0];
};

static void destroyTaskLocal(void* data) {
    TaskLocal* local = (TaskLocal*)data;
    while (local) {
        TaskLocal* next = local->next;
        if (local->destructor)
            local->destructor(local->data);
        chiFree(local);
        local = next;
    }
}
