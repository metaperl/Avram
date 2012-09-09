#ifndef AVM_MATCON_H
#define AVM_MATCON_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern void *avm_vector_of_list (list operand,size_t item_size,list *message,int *fault);
  extern list avm_list_of_vector (void *vector,int num_items,size_t item_size,int *fault);
  extern void *avm_matrix_of_list (int square, int upper_triangular, int lower_triangular,int column_major,
						    list operand,size_t item_size,list *message,int *fault);
  extern list avm_list_of_matrix (void *matrix,int rows,int cols,size_t item_size,int *fault);
  extern void *avm_matrix_transposition  (void *matrix, int rows, int cols, size_t item_size);
  extern void *avm_matrix_reflection  (int upper_triangular, void *matrix, int n, size_t item_size);
  extern list avm_list_of_packed_matrix (int upper_trianguler,void *operand,int n,size_t item_size, int *fault);
  extern void *avm_packed_matrix_of_list (int upper_triangular,list operand,int n,size_t item_size,
							   list *message,int *fault);
  extern list *avm_row_number_array (counter m,int *fault);
  extern void avm_dispose_rows (counter m,list *row_number);
  extern void avm_initialize_matcon ();
  extern void avm_count_matcon ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_MATCON_H */
