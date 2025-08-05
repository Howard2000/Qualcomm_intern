#include <stdio.h>
#include <stdlib.h>

void print_array(int *arr, int size) {
    for (int i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int main() {
    int *array = (int *)malloc(5 * sizeof(int));
    if (array == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < 5; i++) {
        array[i] = i + 1;
    }

    printf("Original array: ");
    print_array(array, 5);

    int *ptr = array + 5; 
    *ptr = 10; 

    printf("Modified array: ");
    print_array(array, 5);

    free(array);
    return 0;
}
