#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float x;
    float y;
    float z;
} vertex_t;

typedef struct {
    int vertices_idx[3];
} face_t;

typedef struct
{
    int nb_vertices;
    vertex_t * vertices;
    int nb_faces;
    face_t * faces;
} object_t;

/* The implementation of these functions is located at the end of the file; 
they are not relevant for you. */
void create_object(object_t ** obj);
void clean_object(object_t * obj);
void print_object(object_t *obj);

void flip_and_expand(object_t *obj, float expand_factor_x, float expand_factor_y, float expand_factor_z){    
    // expand
    for (int i =0; i < obj->nb_vertices; i ++){
        obj->vertices[i].x *= expand_factor_x;
        obj->vertices[i].y *= expand_factor_y;
        obj->vertices[i].z *= expand_factor_z;
    }
    // flip
    for (int i =0; i < obj->nb_faces; i ++){
        int tmp = obj->faces[i].vertices_idx[0];
        obj->faces[i].vertices_idx[0] = obj->faces[i].vertices_idx[2];
        obj->faces[i].vertices_idx[2] = tmp;
    }
}

int main()
{
    object_t *obj;
    create_object(&obj);

    printf(">>>>> Avant transformation\n");
    print_object(obj);
    
    flip_and_expand(obj, 2.0, 3.0, 0.5);

    printf(">>>>> Après transformation\n");
    print_object(obj);

    clean_object(obj);
    return 0;
}


void create_object(object_t ** obj){
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    *obj = (object_t *)malloc(sizeof(object_t));    

    // compute number of vertices and faces
    int nb_vertices = 0;
    int nb_faces = 0;
    fp = fopen("cube.obj", "r");
    if (fp == NULL)
        exit(1);
    while ((getline(&line, &len, fp)) != -1) {
        char * token = strtok(line, " ");
        if (*token == 'v')
        {
            nb_vertices ++;            
        }
        else if (*token == 'f')
        {
            nb_faces ++;
        }
    }
    fclose(fp);

    (*obj)->nb_vertices = nb_vertices;
    (*obj)->vertices = (vertex_t *)malloc(sizeof(vertex_t) * nb_vertices);
    (*obj)->nb_faces = nb_faces;
    (*obj)->faces = (face_t *)malloc(sizeof(face_t)*nb_faces);

    // populate vertices and faces
    fp = fopen("cube.obj", "r");
    line = NULL;
    int idx_vertex = 0;
    int idx_face = 0;
    while ((getline(&line, &len, fp)) != -1) {
        char * token = strtok(line, " ");
        if (*token == 'v')
        {
            token = strtok(NULL, " ");            
            (((*obj)->vertices)[idx_vertex]).x = atof(token);                
            token = strtok(NULL, " ");            
            (((*obj)->vertices)[idx_vertex]).y = atof(token);                
            token = strtok(NULL, " ");            
            (((*obj)->vertices)[idx_vertex]).z = atof(token);
            idx_vertex ++;
        }
        else if (*token == 'f')
        {
            token = strtok(NULL, " ");            
            for (int i =0; i < 3; i ++)
            {
                ((((*obj)->faces)[idx_face]).vertices_idx)[i] = atoi(token);                                                
                token = strtok(NULL, " ");
            }
            idx_face ++;
        }
    }
    if (ferror(fp)) {
        /* handle error */
    }
    fclose(fp);
}

void clean_object(object_t * obj) {
    free(obj->vertices);
    free(obj->faces);
    free(obj);
}

void print_object(object_t *obj)
{
    printf("vertices %d\n",obj->nb_vertices);
    for (int i =0; i < obj->nb_vertices; i ++){
        printf("v %f %f %f\n", obj->vertices[i].x, obj->vertices[i].y, obj->vertices[i].z);
    }
    printf("faces %d\n",obj->nb_faces);
    for (int i =0; i < obj->nb_faces; i ++){
        printf("f %d %d %d\n", obj->faces[i].vertices_idx[0], obj->faces[i].vertices_idx[1], obj->faces[i].vertices_idx[2]);
    }
}