#ifndef COPOSIT_IMPROVED_NBC_MINISAT_API_H
#define COPOSIT_IMPROVED_NBC_MINISAT_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct solver_t improved_nbc_solver;

typedef int (*improved_nbc_model_callback)(void* state, const signed char* assignments, int variable_count);
typedef int (*improved_nbc_terminate_callback)(void* state);

enum {
    IMPROVED_NBC_ENUM_EXHAUSTED = 0,
    IMPROVED_NBC_ENUM_STOPPED = 1,
    IMPROVED_NBC_ENUM_INTERRUPTED = 2,
    IMPROVED_NBC_ENUM_ERROR = 3
};

improved_nbc_solver* improved_nbc_solver_new(void);
void improved_nbc_solver_delete(improved_nbc_solver* solver);
void improved_nbc_solver_set_variable_count(improved_nbc_solver* solver, int variable_count);
int improved_nbc_solver_add_clause(improved_nbc_solver* solver, const int* literals, int count);
int improved_nbc_solver_enumerate(improved_nbc_solver* solver, const int* assumptions, int assumption_count,
                                  improved_nbc_model_callback model_callback, void* model_state,
                                  improved_nbc_terminate_callback terminate_callback, void* terminate_state);
int improved_nbc_solver_is_inconsistent(improved_nbc_solver* solver);

#ifdef __cplusplus
}
#endif

#endif
