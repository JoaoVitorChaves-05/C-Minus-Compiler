#include "tm_codegen.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    TM_AC = 0,
    TM_AC1 = 1,
    TM_TMP = 2,
    TM_GP = 5,
    TM_FP = 6,
    TM_PC = 7
};

typedef enum {
    STORAGE_SCALAR,
    STORAGE_ARRAY,
    STORAGE_ARRAY_PARAMETER
} StorageKind;

typedef struct {
    char *name;
    StorageKind kind;
    int offset;
    int size;
    int is_global;
} Storage;

typedef struct {
    char *name;
    int is_array;
} Parameter;

typedef struct {
    char *name;
    size_t first_quad;
    size_t last_quad;
    Storage *storage;
    size_t storage_count;
    size_t storage_capacity;
    Parameter *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    int frame_slots;
} Function;

typedef struct {
    char *name;
    size_t address;
} Label;

typedef enum {
    TARGET_NONE,
    TARGET_ABSOLUTE,
    TARGET_PC_RELATIVE
} TargetMode;

typedef struct {
    char op[8];
    int r;
    int d;
    int s;
    char *target;
    TargetMode target_mode;
} Instruction;

typedef struct {
    const GCI *gci;
    Instruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    Label *labels;
    size_t label_count;
    size_t label_capacity;
    Storage *globals;
    size_t global_count;
    size_t global_capacity;
    Function *functions;
    size_t function_count;
    size_t function_capacity;
    int global_slots;
    size_t next_return_label;
    int failed;
} TMGenerator;

static char *copy_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text) + 1;
    copy = (char *)malloc(length);
    if (!copy) return NULL;
    memcpy(copy, text, length);
    return copy;
}

static void set_failed(TMGenerator *generator) {
    generator->failed = 1;
}

static int grow_array(void **array, size_t *capacity, size_t item_size) {
    size_t new_capacity;
    void *resized;
    if (*capacity > (size_t)-1 / 2) return 0;
    new_capacity = *capacity ? *capacity * 2 : 16;
    if (new_capacity > (size_t)-1 / item_size) return 0;
    resized = realloc(*array, new_capacity * item_size);
    if (!resized) return 0;
    *array = resized;
    *capacity = new_capacity;
    return 1;
}

static Function *find_function(TMGenerator *generator, const char *name) {
    size_t index;
    for (index = 0; index < generator->function_count; ++index) {
        if (strcmp(generator->functions[index].name, name) == 0) {
            return &generator->functions[index];
        }
    }
    return NULL;
}

static Storage *find_storage(
    Storage *storage,
    size_t count,
    const char *name
) {
    size_t index;
    for (index = 0; index < count; ++index) {
        if (strcmp(storage[index].name, name) == 0) return &storage[index];
    }
    return NULL;
}

static Storage *find_operand_storage(
    TMGenerator *generator,
    Function *function,
    const char *name
) {
    Storage *storage;
    if (function) {
        storage = find_storage(
            function->storage, function->storage_count, name
        );
        if (storage) return storage;
    }
    return find_storage(generator->globals, generator->global_count, name);
}

static int add_storage(
    TMGenerator *generator,
    Storage **storage,
    size_t *count,
    size_t *capacity,
    const char *name,
    StorageKind kind,
    int offset,
    int size,
    int is_global
) {
    Storage *entry;
    if (find_storage(*storage, *count, name)) {
        set_failed(generator);
        return 0;
    }
    if (*count == *capacity &&
        !grow_array((void **)storage, capacity, sizeof(Storage))) {
        set_failed(generator);
        return 0;
    }
    entry = &(*storage)[(*count)++];
    entry->name = copy_string(name);
    entry->kind = kind;
    entry->offset = offset;
    entry->size = size;
    entry->is_global = is_global;
    if (!entry->name) {
        set_failed(generator);
        return 0;
    }
    return 1;
}

static int add_function(TMGenerator *generator, const char *name, size_t quad) {
    Function *function;
    if (find_function(generator, name)) {
        set_failed(generator);
        return 0;
    }
    if (generator->function_count == generator->function_capacity &&
        !grow_array(
            (void **)&generator->functions,
            &generator->function_capacity,
            sizeof(Function)
        )) {
        set_failed(generator);
        return 0;
    }
    function = &generator->functions[generator->function_count++];
    memset(function, 0, sizeof(*function));
    function->name = copy_string(name);
    function->first_quad = quad;
    function->last_quad = quad;
    function->frame_slots = 2;
    if (!function->name) {
        set_failed(generator);
        return 0;
    }
    return 1;
}

static int add_parameter(
    TMGenerator *generator,
    Function *function,
    const char *name,
    int is_array
) {
    Parameter *parameter;
    if (function->parameter_count == function->parameter_capacity &&
        !grow_array(
            (void **)&function->parameters,
            &function->parameter_capacity,
            sizeof(Parameter)
        )) {
        set_failed(generator);
        return 0;
    }
    parameter = &function->parameters[function->parameter_count++];
    parameter->name = copy_string(name);
    parameter->is_array = is_array;
    if (!parameter->name) {
        set_failed(generator);
        return 0;
    }
    return 1;
}

static int add_label(
    TMGenerator *generator,
    const char *name,
    size_t address
) {
    size_t index;
    Label *label;
    for (index = 0; index < generator->label_count; ++index) {
        if (strcmp(generator->labels[index].name, name) == 0) {
            generator->labels[index].address = address;
            return 1;
        }
    }
    if (generator->label_count == generator->label_capacity &&
        !grow_array(
            (void **)&generator->labels,
            &generator->label_capacity,
            sizeof(Label)
        )) {
        set_failed(generator);
        return 0;
    }
    label = &generator->labels[generator->label_count++];
    label->name = copy_string(name);
    label->address = address;
    if (!label->name) {
        set_failed(generator);
        return 0;
    }
    return 1;
}

static int emit_instruction(
    TMGenerator *generator,
    const char *op,
    int r,
    int d,
    int s,
    const char *target,
    TargetMode target_mode
) {
    Instruction *instruction;
    if (generator->instruction_count == generator->instruction_capacity &&
        !grow_array(
            (void **)&generator->instructions,
            &generator->instruction_capacity,
            sizeof(Instruction)
        )) {
        set_failed(generator);
        return 0;
    }
    instruction = &generator->instructions[generator->instruction_count++];
    memset(instruction, 0, sizeof(*instruction));
    if (strlen(op) >= sizeof(instruction->op)) {
        set_failed(generator);
        return 0;
    }
    strcpy(instruction->op, op);
    instruction->r = r;
    instruction->d = d;
    instruction->s = s;
    instruction->target_mode = target_mode;
    if (target) {
        instruction->target = copy_string(target);
        if (!instruction->target) {
            set_failed(generator);
            return 0;
        }
    }
    return 1;
}

static int emit_rm(
    TMGenerator *generator,
    const char *op,
    int r,
    int d,
    int s
) {
    return emit_instruction(generator, op, r, d, s, NULL, TARGET_NONE);
}

static int emit_target(
    TMGenerator *generator,
    const char *op,
    int r,
    const char *target,
    TargetMode mode,
    int s
) {
    return emit_instruction(generator, op, r, 0, s, target, mode);
}

static int add_frame_storage(
    TMGenerator *generator,
    Function *function,
    const char *name,
    StorageKind kind,
    int size
) {
    if (size <= 0 || function->frame_slots > INT_MAX - size) {
        set_failed(generator);
        return 0;
    }
    if (!add_storage(
            generator,
            &function->storage,
            &function->storage_count,
            &function->storage_capacity,
            name,
            kind,
            function->frame_slots,
            size,
            0)) {
        return 0;
    }
    function->frame_slots += size;
    return 1;
}

static int add_global_storage(
    TMGenerator *generator,
    const char *name,
    StorageKind kind,
    int size
) {
    if (size <= 0 || generator->global_slots > INT_MAX - size - 1) {
        set_failed(generator);
        return 0;
    }
    ++generator->global_slots;
    if (!add_storage(
            generator,
            &generator->globals,
            &generator->global_count,
            &generator->global_capacity,
            name,
            kind,
            generator->global_slots,
            size,
            1)) {
        return 0;
    }
    generator->global_slots += size - 1;
    return 1;
}

static int parse_positive_size(const char *text, int *size) {
    char *end;
    long parsed;
    if (!text || !*text) return 0;
    parsed = strtol(text, &end, 10);
    if (*end || parsed <= 0 || parsed > INT_MAX) return 0;
    *size = (int)parsed;
    return 1;
}

static int is_temporary_name(const char *name) {
    return name && name[0] == '$' && name[1] == 't';
}

static int add_temporary_if_needed(
    TMGenerator *generator,
    Function *function,
    const char *name
) {
    if (!is_temporary_name(name) ||
        find_storage(function->storage, function->storage_count, name)) {
        return 1;
    }
    return add_frame_storage(
        generator, function, name, STORAGE_SCALAR, 1
    );
}

static int index_layout(TMGenerator *generator) {
    size_t index;
    Function *current = NULL;

    for (index = 0; index < generator->gci->count; ++index) {
        const Quad *quad = &generator->gci->quads[index];
        if (strcmp(quad->op, "FUN") == 0) {
            if (!add_function(generator, quad->arg2, index)) return 0;
            current = &generator->functions[generator->function_count - 1];
        } else if (strcmp(quad->op, "END") == 0) {
            if (!current) {
                set_failed(generator);
                return 0;
            }
            current->last_quad = index;
            current = NULL;
        } else if (strcmp(quad->op, "ALLOC") == 0) {
            if (strcmp(quad->arg2, "global") == 0) {
                if (!add_global_storage(
                        generator, quad->arg1, STORAGE_SCALAR, 1)) return 0;
            } else {
                if (!current ||
                    !add_frame_storage(
                        generator, current, quad->arg1, STORAGE_SCALAR, 1)) {
                    set_failed(generator);
                    return 0;
                }
            }
        } else if (strcmp(quad->op, "ALLOC_ARRAY") == 0) {
            int size;
            if (!parse_positive_size(quad->arg2, &size)) {
                set_failed(generator);
                return 0;
            }
            if (strcmp(quad->res, "global") == 0) {
                if (!add_global_storage(
                        generator, quad->arg1, STORAGE_ARRAY, size)) return 0;
            } else {
                if (!current ||
                    !add_frame_storage(
                        generator, current, quad->arg1, STORAGE_ARRAY, size)) {
                    set_failed(generator);
                    return 0;
                }
            }
        } else if (strcmp(quad->op, "ARG") == 0 ||
                   strcmp(quad->op, "ARG_ARRAY") == 0) {
            if (!current ||
                !add_parameter(
                    generator,
                    current,
                    quad->arg2,
                    strcmp(quad->op, "ARG_ARRAY") == 0) ||
                !add_frame_storage(
                    generator,
                    current,
                    quad->arg2,
                    strcmp(quad->op, "ARG_ARRAY") == 0
                        ? STORAGE_ARRAY_PARAMETER
                        : STORAGE_SCALAR,
                    1)) {
                set_failed(generator);
                return 0;
            }
        }
    }
    if (current) {
        set_failed(generator);
        return 0;
    }

    for (index = 0; index < generator->function_count; ++index) {
        Function *function = &generator->functions[index];
        size_t quad_index;
        for (quad_index = function->first_quad;
             quad_index <= function->last_quad;
             ++quad_index) {
            const Quad *quad = &generator->gci->quads[quad_index];
            if (!add_temporary_if_needed(generator, function, quad->arg1) ||
                !add_temporary_if_needed(generator, function, quad->arg2) ||
                !add_temporary_if_needed(generator, function, quad->res)) {
                return 0;
            }
        }
    }
    return !generator->failed;
}

static size_t label_address(
    const TMGenerator *generator,
    const char *name,
    int *found
) {
    size_t index;
    for (index = 0; index < generator->label_count; ++index) {
        if (strcmp(generator->labels[index].name, name) == 0) {
            *found = 1;
            return generator->labels[index].address;
        }
    }
    *found = 0;
    return 0;
}

static int add_generated_label(TMGenerator *generator, const char *name) {
    return add_label(generator, name, generator->instruction_count);
}

static int is_integer_operand(const char *operand) {
    const unsigned char *cursor = (const unsigned char *)operand;
    if (!cursor || !*cursor) return 0;
    if (*cursor == '-') ++cursor;
    if (!*cursor) return 0;
    for (; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return 0;
    }
    return 1;
}

static int frame_offset(const Storage *storage, int array_base) {
    if (array_base && storage->kind == STORAGE_ARRAY) {
        return -(storage->offset + storage->size);
    }
    return -(storage->offset + 1);
}

static int emit_load_operand(
    TMGenerator *generator,
    Function *function,
    const char *operand,
    int register_number
) {
    Storage *storage;
    long value;
    char *end;
    if (!operand || strcmp(operand, "-") == 0) {
        set_failed(generator);
        return 0;
    }
    if (is_integer_operand(operand)) {
        value = strtol(operand, &end, 10);
        if (*end || value < INT_MIN || value > INT_MAX) {
            set_failed(generator);
            return 0;
        }
        return emit_rm(generator, "LDC", register_number, (int)value, 0);
    }

    if (operand[0] == '&') {
        storage = find_operand_storage(generator, function, operand + 1);
        if (!storage || storage->kind == STORAGE_SCALAR) {
            set_failed(generator);
            return 0;
        }
        if (storage->kind == STORAGE_ARRAY_PARAMETER) {
            return emit_rm(
                generator, "LD", register_number,
                frame_offset(storage, 0), TM_FP
            );
        }
        return emit_rm(
            generator,
            "LDA",
            register_number,
            storage->is_global
                ? storage->offset
                : frame_offset(storage, storage->kind == STORAGE_ARRAY),
            storage->is_global ? TM_GP : TM_FP
        );
    }

    storage = find_operand_storage(generator, function, operand);
    if (!storage || storage->kind == STORAGE_ARRAY) {
        set_failed(generator);
        return 0;
    }
    return emit_rm(
        generator,
        "LD",
        register_number,
        storage->is_global ? storage->offset : frame_offset(storage, 0),
        storage->is_global ? TM_GP : TM_FP
    );
}

static int emit_store_operand(
    TMGenerator *generator,
    Function *function,
    const char *destination,
    int register_number
) {
    Storage *storage = find_operand_storage(
        generator, function, destination
    );
    if (!storage || storage->kind != STORAGE_SCALAR) {
        set_failed(generator);
        return 0;
    }
    return emit_rm(
        generator,
        "ST",
        register_number,
        storage->is_global ? storage->offset : frame_offset(storage, 0),
        storage->is_global ? TM_GP : TM_FP
    );
}

static int emit_array_base(
    TMGenerator *generator,
    Function *function,
    const char *name,
    int register_number
) {
    Storage *storage = find_operand_storage(generator, function, name);
    if (!storage || storage->kind == STORAGE_SCALAR) {
        set_failed(generator);
        return 0;
    }
    if (storage->kind == STORAGE_ARRAY_PARAMETER) {
        return emit_rm(
            generator,
            "LD",
            register_number,
            frame_offset(storage, 0),
            TM_FP
        );
    }
    return emit_rm(
        generator,
        "LDA",
        register_number,
        storage->is_global
            ? storage->offset
            : frame_offset(storage, 1),
        storage->is_global ? TM_GP : TM_FP
    );
}

static const char *comparison_jump(const char *operation) {
    if (strcmp(operation, "<") == 0) return "JLT";
    if (strcmp(operation, "<=") == 0) return "JLE";
    if (strcmp(operation, ">") == 0) return "JGT";
    if (strcmp(operation, ">=") == 0) return "JGE";
    if (strcmp(operation, "==") == 0) return "JEQ";
    if (strcmp(operation, "!=") == 0) return "JNE";
    return NULL;
}

static int emit_comparison(
    TMGenerator *generator,
    Function *function,
    const Quad *quad
) {
    const char *jump = comparison_jump(quad->op);
    char true_label[48];
    char end_label[48];
    if (!jump) {
        set_failed(generator);
        return 0;
    }
    snprintf(
        true_label, sizeof(true_label), "@cmp_true_%zu",
        generator->next_return_label
    );
    snprintf(
        end_label, sizeof(end_label), "@cmp_end_%zu",
        generator->next_return_label++
    );

    return emit_load_operand(generator, function, quad->arg1, TM_AC) &&
        emit_load_operand(generator, function, quad->arg2, TM_AC1) &&
        emit_rm(generator, "SUB", TM_AC, TM_AC, TM_AC1) &&
        emit_target(
            generator, jump, TM_AC, true_label, TARGET_PC_RELATIVE, TM_PC
        ) &&
        emit_rm(generator, "LDC", TM_AC1, 0, 0) &&
        emit_store_operand(generator, function, quad->res, TM_AC1) &&
        emit_target(
            generator, "LDC", TM_PC, end_label, TARGET_ABSOLUTE, 0
        ) &&
        add_generated_label(generator, true_label) &&
        emit_rm(generator, "LDC", TM_AC1, 1, 0) &&
        emit_store_operand(generator, function, quad->res, TM_AC1) &&
        add_generated_label(generator, end_label);
}

static int emit_array_read(
    TMGenerator *generator,
    Function *function,
    const Quad *quad
) {
    return emit_load_operand(generator, function, quad->arg2, TM_AC1) &&
        emit_array_base(generator, function, quad->arg1, TM_TMP) &&
        emit_rm(generator, "ADD", TM_TMP, TM_TMP, TM_AC1) &&
        emit_rm(generator, "LD", TM_AC, 0, TM_TMP) &&
        emit_store_operand(generator, function, quad->res, TM_AC);
}

static int emit_array_write(
    TMGenerator *generator,
    Function *function,
    const Quad *quad
) {
    return emit_load_operand(generator, function, quad->res, TM_AC) &&
        emit_load_operand(generator, function, quad->arg2, TM_AC1) &&
        emit_array_base(generator, function, quad->arg1, TM_TMP) &&
        emit_rm(generator, "ADD", TM_TMP, TM_TMP, TM_AC1) &&
        emit_rm(generator, "ST", TM_AC, 0, TM_TMP);
}

static int emit_return(TMGenerator *generator, Function *function) {
    if (strcmp(function->name, "main") == 0) {
        return emit_rm(generator, "HALT", 0, 0, 0);
    }
    return emit_rm(generator, "LD", TM_TMP, -1, TM_FP) &&
        emit_rm(generator, "LD", TM_AC1, -2, TM_FP) &&
        emit_rm(generator, "LDA", TM_FP, 0, TM_TMP) &&
        emit_rm(generator, "LDA", TM_PC, 0, TM_AC1);
}

static int emit_call(
    TMGenerator *generator,
    Function *caller,
    const Quad *quad,
    char **arguments,
    size_t argument_count
) {
    Function *callee;
    char return_label[48];
    size_t index;
    int child_offset;

    if (strcmp(quad->arg1, "input") == 0) {
        if (argument_count != 0 ||
            !emit_rm(generator, "IN", TM_AC, 0, 0)) {
            set_failed(generator);
            return 0;
        }
        return strcmp(quad->res, "-") == 0
            ? 1
            : emit_store_operand(generator, caller, quad->res, TM_AC);
    }
    if (strcmp(quad->arg1, "output") == 0) {
        if (argument_count != 1 ||
            !emit_load_operand(generator, caller, arguments[0], TM_AC) ||
            !emit_rm(generator, "OUT", TM_AC, 0, 0)) {
            set_failed(generator);
            return 0;
        }
        return 1;
    }

    callee = find_function(generator, quad->arg1);
    if (!callee || argument_count != callee->parameter_count ||
        caller->frame_slots > INT_MAX) {
        set_failed(generator);
        return 0;
    }
    snprintf(
        return_label,
        sizeof(return_label),
        "@return_%zu",
        generator->next_return_label++
    );
    child_offset = -caller->frame_slots;

    if (!emit_rm(generator, "LDA", TM_TMP, child_offset, TM_FP) ||
        !emit_rm(generator, "ST", TM_FP, -1, TM_TMP) ||
        !emit_target(
            generator, "LDC", TM_AC1, return_label, TARGET_ABSOLUTE, 0
        ) ||
        !emit_rm(generator, "ST", TM_AC1, -2, TM_TMP)) {
        return 0;
    }

    for (index = 0; index < argument_count; ++index) {
        Storage *parameter = find_storage(
            callee->storage,
            callee->storage_count,
            callee->parameters[index].name
        );
        if (!parameter ||
            !emit_load_operand(
                generator, caller, arguments[index], TM_AC
            ) ||
            !emit_rm(
                generator,
                "ST",
                TM_AC,
                frame_offset(parameter, 0),
                TM_TMP
            )) {
            set_failed(generator);
            return 0;
        }
    }

    return emit_rm(generator, "LDA", TM_FP, child_offset, TM_FP) &&
        emit_target(
            generator, "LDC", TM_PC, callee->name, TARGET_ABSOLUTE, 0
        ) &&
        add_generated_label(generator, return_label) &&
        (strcmp(quad->res, "-") == 0
            ? 1
            : emit_store_operand(generator, caller, quad->res, TM_AC));
}

static int emit_quad(
    TMGenerator *generator,
    Function **current_function,
    char ***pending_arguments,
    size_t *pending_count,
    size_t *pending_capacity,
    const Quad *quad
) {
    Function *function = *current_function;

    if (strcmp(quad->op, "FUN") == 0) {
        function = find_function(generator, quad->arg2);
        if (!function ||
            !add_generated_label(generator, function->name)) {
            set_failed(generator);
            return 0;
        }
        *current_function = function;
        return 1;
    }
    if (strcmp(quad->op, "LAB") == 0) {
        return add_generated_label(generator, quad->arg1);
    }
    if (strcmp(quad->op, "ALLOC") == 0 ||
        strcmp(quad->op, "ALLOC_ARRAY") == 0 ||
        strcmp(quad->op, "ARG") == 0 ||
        strcmp(quad->op, "ARG_ARRAY") == 0) {
        return 1;
    }
    if (strcmp(quad->op, "HALT") == 0) {
        return emit_rm(generator, "HALT", 0, 0, 0);
    }
    if (!function) {
        set_failed(generator);
        return 0;
    }
    if (strcmp(quad->op, "PARAM") == 0) {
        char **resized;
        size_t new_capacity;
        if (*pending_count == *pending_capacity) {
            new_capacity = *pending_capacity ? *pending_capacity * 2 : 4;
            if (new_capacity > (size_t)-1 / sizeof(char *)) {
                set_failed(generator);
                return 0;
            }
            resized = (char **)realloc(
                *pending_arguments, new_capacity * sizeof(char *)
            );
            if (!resized) {
                set_failed(generator);
                return 0;
            }
            *pending_arguments = resized;
            *pending_capacity = new_capacity;
        }
        (*pending_arguments)[*pending_count] = copy_string(quad->arg1);
        if (!(*pending_arguments)[*pending_count]) {
            set_failed(generator);
            return 0;
        }
        ++*pending_count;
        return 1;
    }
    if (strcmp(quad->op, "CALL") == 0) {
        int success = emit_call(
            generator,
            function,
            quad,
            *pending_arguments,
            *pending_count
        );
        size_t index;
        for (index = 0; index < *pending_count; ++index) {
            free((*pending_arguments)[index]);
        }
        *pending_count = 0;
        return success;
    }
    if (*pending_count != 0) {
        set_failed(generator);
        return 0;
    }

    if (strcmp(quad->op, "=") == 0) {
        return emit_load_operand(generator, function, quad->arg1, TM_AC) &&
            emit_store_operand(generator, function, quad->res, TM_AC);
    }
    if (strcmp(quad->op, "[]") == 0) {
        return emit_array_read(generator, function, quad);
    }
    if (strcmp(quad->op, "[]=") == 0) {
        return emit_array_write(generator, function, quad);
    }
    if (comparison_jump(quad->op)) {
        return emit_comparison(generator, function, quad);
    }
    if (strcmp(quad->op, "+") == 0 ||
        strcmp(quad->op, "-") == 0 ||
        strcmp(quad->op, "*") == 0 ||
        strcmp(quad->op, "/") == 0) {
        const char *operation = strcmp(quad->op, "+") == 0 ? "ADD"
            : strcmp(quad->op, "-") == 0 ? "SUB"
            : strcmp(quad->op, "*") == 0 ? "MUL"
            : "DIV";
        return emit_load_operand(generator, function, quad->arg1, TM_AC) &&
            emit_load_operand(generator, function, quad->arg2, TM_AC1) &&
            emit_rm(generator, operation, TM_AC, TM_AC, TM_AC1) &&
            emit_store_operand(generator, function, quad->res, TM_AC);
    }
    if (strcmp(quad->op, "IFF") == 0) {
        return emit_load_operand(generator, function, quad->arg1, TM_AC) &&
            emit_target(
                generator,
                "JEQ",
                TM_AC,
                quad->res,
                TARGET_PC_RELATIVE,
                TM_PC
            );
    }
    if (strcmp(quad->op, "GOTO") == 0) {
        return emit_target(
            generator,
            "LDC",
            TM_PC,
            quad->arg1,
            TARGET_ABSOLUTE,
            0
        );
    }
    if (strcmp(quad->op, "RET") == 0) {
        int success = strcmp(quad->arg1, "-") == 0
            ? emit_rm(generator, "LDC", TM_AC, 0, 0)
            : emit_load_operand(generator, function, quad->arg1, TM_AC);
        return success && emit_return(generator, function);
    }
    if (strcmp(quad->op, "END") == 0) {
        int is_main = function == find_function(generator, "main");
        int success = is_main
            ? 1
            : emit_rm(generator, "LDC", TM_AC, 0, 0) &&
                emit_return(generator, function);
        *current_function = NULL;
        return success;
    }

    set_failed(generator);
    return 0;
}

static int resolve_targets(TMGenerator *generator) {
    size_t index;
    for (index = 0; index < generator->instruction_count; ++index) {
        Instruction *instruction = &generator->instructions[index];
        size_t address;
        int found;
        long offset;
        if (instruction->target_mode == TARGET_NONE) continue;
        address = label_address(generator, instruction->target, &found);
        if (!found || address > INT_MAX) {
            set_failed(generator);
            return 0;
        }
        if (instruction->target_mode == TARGET_ABSOLUTE) {
            instruction->d = (int)address;
        } else {
            offset = (long)address - ((long)index + 1);
            if (offset < INT_MIN || offset > INT_MAX) {
                set_failed(generator);
                return 0;
            }
            instruction->d = (int)offset;
        }
    }
    return !generator->failed;
}

static void destroy_generator(TMGenerator *generator) {
    size_t index;
    for (index = 0; index < generator->global_count; ++index) {
        free(generator->globals[index].name);
    }
    for (index = 0; index < generator->function_count; ++index) {
        Function *function = &generator->functions[index];
        size_t inner;
        free(function->name);
        for (inner = 0; inner < function->storage_count; ++inner) {
            free(function->storage[inner].name);
        }
        for (inner = 0; inner < function->parameter_count; ++inner) {
            free(function->parameters[inner].name);
        }
        free(function->storage);
        free(function->parameters);
    }
    for (index = 0; index < generator->label_count; ++index) {
        free(generator->labels[index].name);
    }
    for (index = 0; index < generator->instruction_count; ++index) {
        free(generator->instructions[index].target);
    }
    free(generator->globals);
    free(generator->functions);
    free(generator->labels);
    free(generator->instructions);
}

int generate_tm(FILE *output, const GCI *gci) {
    TMGenerator generator;
    Function *current_function = NULL;
    char **pending_arguments = NULL;
    size_t pending_count = 0;
    size_t pending_capacity = 0;
    size_t index;
    int success = 0;

    if (!output || !gci || !gci->quads || gci->count == 0) return 0;
    memset(&generator, 0, sizeof(generator));
    generator.gci = gci;
    if (!index_layout(&generator) ||
        !find_function(&generator, "main")) {
        set_failed(&generator);
        goto cleanup;
    }

    if (!emit_rm(&generator, "LDC", TM_GP, 0, 0) ||
        !emit_rm(&generator, "LD", TM_FP, 0, 0) ||
        !emit_target(
            &generator, "LDC", TM_PC, "main", TARGET_ABSOLUTE, 0
        )) {
        goto cleanup;
    }

    for (index = 0; index < gci->count; ++index) {
        if (!emit_quad(
                &generator,
                &current_function,
                &pending_arguments,
                &pending_count,
                &pending_capacity,
                &gci->quads[index])) {
            goto cleanup;
        }
    }
    if (pending_count != 0 || !resolve_targets(&generator)) goto cleanup;

    for (index = 0; index < generator.instruction_count; ++index) {
        const Instruction *instruction = &generator.instructions[index];
        if (strcmp(instruction->op, "HALT") == 0 ||
            strcmp(instruction->op, "IN") == 0 ||
            strcmp(instruction->op, "OUT") == 0 ||
            strcmp(instruction->op, "ADD") == 0 ||
                   strcmp(instruction->op, "SUB") == 0 ||
                   strcmp(instruction->op, "MUL") == 0 ||
                   strcmp(instruction->op, "DIV") == 0) {
            if (fprintf(
                    output,
                    "%zu: %s %d,%d,%d\n",
                    index,
                    instruction->op,
                    instruction->r,
                    instruction->d,
                    instruction->s
                ) < 0) {
                goto cleanup;
            }
        } else {
            if (fprintf(
                    output,
                    "%zu: %s %d,%d(%d)\n",
                    index,
                    instruction->op,
                    instruction->r,
                    instruction->d,
                    instruction->s
                ) < 0) {
                goto cleanup;
            }
        }
    }
    success = !ferror(output);

cleanup:
    for (index = 0; index < pending_count; ++index) {
        free(pending_arguments[index]);
    }
    free(pending_arguments);
    destroy_generator(&generator);
    return success;
}
