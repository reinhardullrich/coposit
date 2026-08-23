#ifndef COPOSIT_NBC_MINISAT_API_H
#define COPOSIT_NBC_MINISAT_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct solver_t nbc_solver;

typedef int (*nbc_model_callback)(void* state, const signed char* assignments, int variable_count);
typedef int (*nbc_terminate_callback)(void* state);

enum {
    NBC_ENUM_EXHAUSTED = 0,
    NBC_ENUM_STOPPED = 1,
    NBC_ENUM_INTERRUPTED = 2,
    NBC_ENUM_ERROR = 3
};

nbc_solver* nbc_solver_new(void);
void nbc_solver_delete(nbc_solver* solver);
void nbc_solver_set_variable_count(nbc_solver* solver, int variable_count);
// Returns 1 on success, 0 when the clause makes the formula inconsistent, and -1 on API/allocation error.
int nbc_solver_add_clause(nbc_solver* solver, const int* literals, int count);
int nbc_solver_enumerate(nbc_solver* solver, const int* assumptions, int assumption_count,
                         nbc_model_callback model_callback, void* model_state,
                         nbc_terminate_callback terminate_callback, void* terminate_state);

#ifdef __cplusplus
}
#endif

#endif
