//
// Created by antoniomusolino on 19/01/18.
//

#define YGL_OPENGL 1
#include "yocto-gl/yocto/yocto_gl.h"
using namespace ygl;

enum angles {
    _0 =0,
    _90 =90,
    _180 = 180,
    _270 = 270,
};

struct Move {
    vec3f Left = {};
    vec3f Right = {};
    vec3f Forward = {};
    vec3f Back = {};
    vec3f NotMove = {};

};


struct position_matrix {
    vector<bool>* pos;
    unsigned int h;
    unsigned int w;
    position_matrix(unsigned int height, unsigned int width) {
        h = height;
        w = width;
        pos = new vector<bool>(height * width);
    }
    bool at (unsigned int x, unsigned int y) {
        if (x >= w || y >= h) throw false;
        return pos->at(y * w + x );
    }
    void set (unsigned int x, unsigned int y, bool value) {
        if (x >= w || y >= h) throw false;
        pos->at(y * w + x ) = value;
    }
    bool isGoodPos (unsigned int x, unsigned int y) {
        return !(x >= w || y >= h);
    }
};



struct transform {
    vec3f move ;
    vec3f constanValue = {0,0,0};
    angles rotation = _0;
    vec3f scale = {0,0,0};
};

struct edge {
    long indexNode;
    transform transf;
};

struct node {
    bool isTerminal = false;
    vector<vector<edge>*> adj;
    vector<shape*> shapes;
    long graphPos = -1;
};

struct  Graph {
    vector<node> nodes;
    long nodeStart = -1;
    Move unit = {
            {0,0,-1},
            {0,0,1},
            {1,0,0},
            {-1,0,0},
            {0,0,0}
    };
};

// Add a istance on scene
void add_instance(scene* scn, const std::string& name, const frame3f& f,
                  shape* shp) {
    auto ist = new instance();
    ist->name = name;
    ist->shp = shp;
    ist->frame = f;
    scn->instances.push_back(ist);
}

// create istances of nodes
void add_node_to_scene (scene* scn, const node& nod, const frame3f& f) {
    for (auto shape : nod.shapes) {
        add_instance(scn,"Instance:" + to_string(scn->instances.size()), f,shape);
    }
}


// Create node and load it shape to scene
node loadNode(const string &filename, scene *scene,
              std::map<string, material *> *mapMat) {
    auto object = load_obj(filename);
    for (auto mat : object->materials) {
        if ((*mapMat)[mat->name] != nullptr ) continue;
        auto mater = new material();
        mater->name = mat->name;
        mater->kd = mat->kd;
        //mater->ks = mat->ks;
        //mater->kt = mat->kt;
        //mater->kr = mat->kr; // le mesh sono troppo riflettenti
        //mater->ke = mat->ke;
        (*mapMat)[mat->name] = mater;
        scene->materials.push_back(mater);
    }
    auto nod = node{};

    for (auto obj :  object->objects) {
        for (auto mesh : get_mesh(object,*obj,false)->shapes) {

            auto sh = new shape();
            sh->name = "Shape:" + to_string(scene->shapes.size());
            sh->triangles = mesh.triangles;
            sh->texcoord = mesh.texcoord;
            sh->points = mesh.points;
            sh->pos = mesh.pos;
            sh->mat = (*mapMat)[mesh.matname];
            scene->shapes.push_back(sh);
            //Aggiungo la shape al nodo
            nod.shapes.push_back(sh);

            //add_instance(scene, sh->name, frame, sh);
        }
    }
    return nod;
}
// Add rule " nod := .. | nod1 "
void add_once_node_or(node &nod, node &nod1, transform constValue, Graph *graph) {
    if (nod1.graphPos == -1) {
        nod1.graphPos = graph->nodes.size();
        graph->nodes.push_back(nod1);
    }
    if (nod.graphPos == -1) {
        nod.graphPos = graph->nodes.size();
        graph->nodes.push_back(nod);
    }
    auto node = graph->nodes.at(nod.graphPos);
    auto node1 = graph->nodes.at(nod1.graphPos);

    //Inserisco un nuovo vettore di archi
    node.adj.push_back(new vector<edge>());
    //Inserisco dentro il vettore di archi l'arco con il nodo da aggiungere
    node.adj.at(node.adj.size()-1)->push_back(edge{node1.graphPos,constValue});

    // Mi assicuro che il nodo venga salvato
    graph->nodes.at(node.graphPos) = node;
    //nod.graphPos = node.graphPos;
    //nod1.graphPos = node1.graphPos;
}


void add_multi_nodes_or(node &nod, Graph *graph, vector<pair<node &, transform>> vect){
    for (auto tup : vect) {
        add_once_node_or(nod, tup.first, tup.second, graph);
    }
}

void add_multi_nodes_and(node &nod, Graph *graph, vector<pair<node &, transform>> vect) {
    if (nod.graphPos == -1) {
        nod.graphPos = graph->nodes.size();
        graph->nodes.push_back(nod);
    }
    auto node = graph->nodes.at(nod.graphPos);
    auto vectEdge = new vector<edge>();
    for (auto pair : vect) {
        if (pair.first.graphPos == -1) {
            pair.first.graphPos = graph->nodes.size();
            graph->nodes.push_back(pair.first);
        }

        auto node1 = graph->nodes.at(pair.first.graphPos);
        vectEdge->push_back(edge{node1.graphPos,pair.second});
    }

    node.adj.push_back(vectEdge);
    graph->nodes.at(node.graphPos) = node;

}


angles getNewAngle(angles a1, angles a2) {
    auto newAngles = a1 + a2;
    newAngles = newAngles % 360;
    if (newAngles == _0) return _0;
    else if (newAngles == _90) return _90;
    else if (newAngles == _180) return _180;
    else return _270;
}

/*
 * returns true if there are a collision with subtituions of edge
 */
bool collisionControll (Graph* graph,angles rot, vec2i& matPos, edge edge, position_matrix& position) {

    if(edge.transf.move == graph->unit.NotMove ) return false;

    if (rot == _0) {
        if (edge.transf.move == graph->unit.Right) {
            matPos.x += 1;
        }
        else if (edge.transf.move == graph->unit.Left) {
            matPos.x += -1;
        }
        else if (edge.transf.move == graph->unit.Forward) {
            matPos.y += -1;
        }
        else if (edge.transf.move == graph->unit.Back) {
            matPos.y += 1;
        }
    } else if (rot == _90) {
        if (edge.transf.move == graph->unit.Right) {
            matPos.y += -1;
        }
        else if (edge.transf.move == graph->unit.Left) {
            matPos.y += +1;
        }
        else if (edge.transf.move == graph->unit.Forward) {
            matPos.x += -1;
        }
        else if (edge.transf.move == graph->unit.Back) {
            matPos.x += 1;
        }
    } else if (rot == _180) {
        if (edge.transf.move == graph->unit.Right) {
            matPos.x += -1;
        }
        else if (edge.transf.move == graph->unit.Left) {
            matPos.x += 1;
        }
        else if (edge.transf.move == graph->unit.Forward) {
            matPos.y += 1;
        }
        else if (edge.transf.move == graph->unit.Back) {
            matPos.y += -1;
        }
    } else {
        if (edge.transf.move == graph->unit.Right) {
            matPos.y += 1;
        }
        else if (edge.transf.move == graph->unit.Left) {
            matPos.y += -1;
        }
        else if (edge.transf.move == graph->unit.Forward) {
            matPos.x += 1;
        }
        else if (edge.transf.move == graph->unit.Back) {
            matPos.x += -1;
        }

    }
    return (!position.isGoodPos(matPos.x,matPos.y) || position.at(matPos.x,matPos.y));
}

/*
 * Builds the scene from the grammar
 * inode: it is the starting node.
 * blockPosition: matrix for the collision.
 * matPos: position on the matrix.
 */
void build (scene* scn, Graph* graph, long inode,frame3f pos,rng_pcg32& rng, position_matrix& blockPosition,angles rot, vec2i matPos) {
    auto node = graph->nodes.at(inode);
    if (node.shapes.size() != 0) add_node_to_scene(scn,node, pos);
    if (node.adj.size() == 0 ) return ;
    auto ir = next_rand1i(rng,node.adj.size());
    print("value gen: {} values: {} \n",ir, node.adj.size()-1);
    for (auto edge : *(node.adj.at(ir)) ) {
        auto posMat = matPos;
        if (collisionControll(graph,rot,posMat,edge,blockPosition)) {
            continue;
        }
        blockPosition.set(posMat.x,posMat.y, true);

        auto newPos = pos;
        auto framTrasl = translation_frame3f(edge.transf.constanValue + edge.transf.move);
        newPos = transform_frame(newPos,framTrasl);
        auto newAngle = rot;
        if (edge.transf.scale != vec3f{0,0,0}) {
            auto framScale = scaling_frame3f(edge.transf.scale);
            newPos = transform_frame(newPos,framScale);
        }
        if (edge.transf.rotation != _0) {
            auto angle = edge.transf.rotation * pif / 180.0f;
            auto f = rotation_frame3f({0,1,0}, angle);
            newPos = transform_frame(newPos,f);
            // Riposiziona l'oggetto nella posizione precedente alla rotazione
            auto reposition = frame3f{};
            if (edge.transf.rotation == _90) {
                newAngle = getNewAngle(rot,_90);
                reposition = translation_frame3f({0,0,length(newPos.z)});
            } else if (edge.transf.rotation == _180){
                newAngle = getNewAngle(rot,_180);
                reposition = translation_frame3f({-length(newPos.x),0,length(newPos.z)});
            } else if (edge.transf.rotation == _270) {
                newAngle = getNewAngle(rot,_270);
                reposition = translation_frame3f({-length(newPos.x),0,0});
            }
            newPos = transform_frame(newPos,reposition);
        }
        build(scn,graph, edge.indexNode,newPos,rng,blockPosition,newAngle,posMat);
    }

}

void callBuild(scene* scn, Graph* graph, long inode,
               frame3f pos, rng_pcg32 rng, unsigned int height, unsigned int width) {
    auto blockPosition = position_matrix(height,width);
    build(scn,graph,inode,pos,rng,blockPosition,_0, {int(height-1), int(width-1)});
}

// Create roads rules
void build_roads(scene* scen, std::map<string, material*>* mapMat, Graph* graph) {

    auto stradaConStrisciPedonale =  loadNode("myModels/roadTile_025.obj",scen,mapMat);
    auto stradaConPiccolaUscitaInBassoVerde = loadNode("myModels/roadTile_032.obj",scen,mapMat);
    auto stradaConUscitaGrandeInBassoVerde = loadNode("myModels/roadTile_150.obj",scen,mapMat);
    auto stradaDrittaBordiVerde = loadNode("myModels/roadTile_142.obj",scen,mapMat);
    //auto stradaDrittaSenzaUnBordoVerde= loadNode("ModelsRoads/roadTile_149.obj",scen,mapMat);
    //auto stradaDrittaVerdeRialzata= loadNode("ModelsRoads/roadTile_183.obj",scen,mapMat);
    //auto incrocioAQuattroVerde = loadNode("ModelsRoads/roadTile_141.obj",scen,mapMat);
    //auto bloccoVerdePiano = loadNode("ModelsRoads/roadTile_168.obj",scen,mapMat);
    auto stradaDrittaVerde = loadNode("myModels/roadTile_162.obj",scen,mapMat);
    auto stradaChiusa = loadNode("myModels/roadTile_038.obj",scen,mapMat);


    //auto stradaConPiccolaUscitaInBassoBianca = loadNode("ModelsRoads/roadTile_121.obj",scen,mapMat);
    //auto incrocioAQuattroBianco = loadNode("ModelsRoads/roadTile_121.obj",scen,mapMat);
    //auto stradaStrisceSenzaBianca = loadNode("ModelsRoads/roadTile_109.obj",scen,mapMat);
    //auto stradaDrittaConBordiGrossiBianca = loadNode("ModelsRoads/roadTile_109.obj",scen,mapMat);
    auto stradaDrittaBianca = loadNode("myModels/roadTile_292.obj",scen,mapMat);


    auto albero = loadNode("myModels/roadTile_019.obj",scen,mapMat);
    auto alberoBig = loadNode("myModels/roadTile_020.obj",scen,mapMat);

    auto house = graph->nodes.at(graph->nodeStart);

    auto startNode = node{};
    auto stradeDritte = node{};
    auto stradeConCurve = node{};
    auto alberi = node{};

    add_multi_nodes_or(startNode,graph,{
            {stradaConStrisciPedonale, {{}}},
    });

    auto terminal = node{};

    add_multi_nodes_or(stradeDritte, graph, {
            {stradaDrittaVerde,{}},
            {stradaConStrisciPedonale, {}},
            {stradaDrittaVerde,{}},
            {stradaDrittaVerde,{}},
            {stradaDrittaVerde,{}}

    });
    add_multi_nodes_or(stradeConCurve, graph, {
            {stradaConUscitaGrandeInBassoVerde, {}}
    });
    add_multi_nodes_or(alberi, graph, {
            {albero, {graph->unit.NotMove}},
            {alberoBig,{graph->unit.NotMove,{0,0,0.125f}}}
    });

    // Node Obj
    add_multi_nodes_or(stradaConStrisciPedonale,graph,{
            {stradeDritte, {graph->unit.Forward}},
            {stradeConCurve, {graph->unit.Forward,{0,0,0},_90}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {house,{graph->unit.Left,{0,0.2,0},_90}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {alberi,{graph->unit.NotMove,{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {house,{graph->unit.Left,{0,0.2,0},_90}},
            {alberi,{graph->unit.NotMove,{0.2,0.2,0.11f}}}
    });


    // Strada Normale
    add_multi_nodes_or(stradaDrittaVerde,graph,{
            {stradeDritte, {graph->unit.Forward}},
            {stradeConCurve, {graph->unit.Forward,{0,0,0},_90}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {house,{graph->unit.Left,{0,0.2,0},_90}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {alberi,{graph->unit.NotMove,{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{graph->unit.Forward}},
            {house,{graph->unit.Left,{0,0.2,0},_90}},
            {alberi,{graph->unit.NotMove,{0.2,0.2,0.11f}}}
    });



    add_multi_nodes_and(stradaConUscitaGrandeInBassoVerde,graph,{
            {stradeDritte,{graph->unit.Forward}},
            {stradeDritte,{graph->unit.Right,{0,0,0}, _270}}
    });


    graph->nodeStart = startNode.graphPos;



}

// create houses rules
Graph* build_graph_houses(scene* scen, std::map<string, material*>* mapMat,Graph * graph) {
    //auto graph = new Graph();

    auto startNode = node{};

    // load houses
    auto BaseConScalinata = loadNode("myModels/modularBuildings_027.obj", scen, mapMat);
    auto BaseConScalinataEFinestre = loadNode("myModels/modularBuildings_024.obj", scen, mapMat);
    auto baseConFinestreEPortone = loadNode("myModels/modularBuildings_054.obj", scen, mapMat); //

    // piani
    auto pianoFinestre = loadNode("myModels/modularBuildings_030.obj", scen, mapMat); // Piano con due finestre
    auto pianoFinestrone = loadNode("myModels/modularBuildings_041.obj", scen, mapMat); // piano con finestrona grossa e balconcino
    auto pianoFinestreQuadrate = loadNode("myModels/modularBuildings_034.obj", scen, mapMat);
    auto pianoConBalcone = loadNode("Models/modularBuildings_042.obj", scen, mapMat);
    auto pianoConFinestreCoperte = loadNode("myModels/modularBuildings_033.obj", scen, mapMat);

    // load roofs
    auto tetto = loadNode("Models/modularBuildings_044.obj", scen, mapMat);
    auto tettoConFinestra = loadNode("Models/modularBuildings_063.obj", scen, mapMat);
    auto tettoTriangolo = loadNode("Models/modularBuildings_065.obj", scen, mapMat);
    auto tetto2 = loadNode("Models/modularBuildings_051.obj", scen, mapMat);
    auto tetto3 = loadNode("Models/modularBuildings_064.obj", scen, mapMat);
    auto tetto4 = loadNode("Models/modularBuildings_052.obj", scen, mapMat);


    //finestre
    auto portaAdArco = loadNode("Models/modularBuildings_099.obj", scen, mapMat);



    // Variabili Contenitore
    auto basi = node{};
    auto tetti = node{};
    auto piani = node{};

    add_multi_nodes_or(basi,graph, {
            {BaseConScalinata,  {}},
            {baseConFinestreEPortone, {}},
            {BaseConScalinataEFinestre,{}}
    });

    add_multi_nodes_or(tetti, graph, {
            {tetto, {}},
            {tettoConFinestra, {graph->unit.NotMove,{1, 0, 0}}},
            {tettoTriangolo, {graph->unit.NotMove}},
            //{tetto2,{{1,0,0}}},
            //{tetto3,{{1,0,0}}},
            {tetto4,{graph->unit.NotMove,{1,0,0}}}
    });
    add_multi_nodes_or(piani, graph, {
            {pianoFinestre,  {graph->unit.NotMove}},
            {pianoFinestrone, {graph->unit.NotMove}},
            {pianoFinestreQuadrate, {graph->unit.NotMove}},
            {pianoConBalcone,{graph->unit.NotMove}},
            {pianoConFinestreCoperte,{graph->unit.NotMove}}
    });

    //Metto che la variabile di start parte con una base
    add_multi_nodes_or(startNode, graph, {
            {basi,{graph->unit.NotMove}}
    });


    //add_multi_nodes(n, graph, { {{0,0,0},nr1},{{0,0,0},nr2} } );


    //Varaibile BaseConScalinata
    add_multi_nodes_or(BaseConScalinata, graph, {
            {piani,  {graph->unit.NotMove,{0, 0.8, 0}}},
            {tetti, {graph->unit.NotMove,{0, 0.8, 0}}}
    });

    add_multi_nodes_or(BaseConScalinataEFinestre, graph, {
            {piani,  {graph->unit.NotMove,{0, 0.8, 0}}},
            {tetti, {graph->unit.NotMove,{0, 0.8, 0}}}
    });


    //Variabili piani
    add_multi_nodes_or(pianoFinestre, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani,  {graph->unit.NotMove,{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoFinestrone, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani,  {graph->unit.NotMove,{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoFinestreQuadrate, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani,  {graph->unit.NotMove,{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoConBalcone, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani,  {graph->unit.NotMove,{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoConFinestreCoperte, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani,  {graph->unit.NotMove,{0, 0.6, 0}}}
    });

    // Variabile Base con finestre
    add_multi_nodes_or(baseConFinestreEPortone, graph, {
            {tetti, {graph->unit.NotMove,{0, 0.6, 0}}},
            {piani, {graph->unit.NotMove,{0, 0.6, 0}}}
    });


    //Inserisco la variabile startNode come nodo di partenza
    graph->nodeStart = startNode.graphPos;
    return graph;

}








int main (int argc, char** argv) {
    // command line parser
    auto parser = make_parser(argc, argv, "cities-generator", "create city");
    auto dim = parse_opt(parser, "--dimension", "-d","cities dimension", 50);
    auto out = parse_opt(parser, "--output-file", "-o", "output filename" , "out.obj"s);
    auto sky = parse_flag(parser, "--sky", "-s", "insert sky" , false);

    auto scen = new scene();

    auto mapMat = new std::map<string, material*>();
    //auto mapShape = new std::map<string, shape*>();

    if (sky) {
        auto env = new environment();
        env->name = "sky";
        env->ke = {1, 1, 1};
        auto txt = new texture();
        txt->path = "sky2.hdr";
        txt->hdr = make_sunsky_image(512, pif/2 ,3,true,true);
        env->ke_txt.txt = txt;
        scen->textures += txt;
        scen->environments += env;
    } else {
        // add light
        auto lshp = new shape{"light"};
        lshp->pos = {{1.4f,  8, 6},
                     {-1.4f, 8, 6}};
        lshp->points = {0, 1};
        auto lmat = new material{"light"};
        auto light = 50.0f;
        lmat->ke = {130 * light, 94 * light, 90 * light};
        lmat->kd = {1.0f, 0.57647058823f, 0.16078431372f};
        lshp->mat = lmat;

        scen->shapes.push_back(lshp);
        scen->materials.push_back(lmat);
        scen->instances.push_back(
                new instance{"light", {{1,0,0},{0,1,0},{0,0,1},{0,25,50}}, lshp});
    }
    // add cam
    auto cam = new camera{"cam"};
    cam ->frame = {{1,0,0},{0,1,0},{0,0,1},{-25,10,+25}};
    cam->frame = transform_frame(cam->frame,translation_frame3f({0,0,10}));
    cam->frame = transform_frame(cam->frame,rotation_frame3f({0,1,0},-pif/8));
    cam->frame = transform_frame(cam->frame,rotation_frame3f({1,0,0},-pif/20));
    cam->frame = transform_frame(cam->frame,translation_frame3f({3.8,-4.25f,20}));
    cam->yfov = 15 * pif / 180.f;
    cam->aspect = 16.0f / 9.0f;
    cam->aperture = 0;
    cam->focus = length(vec3f{0, 4, 10} - vec3f{0, 1, 0});
    scen->cameras.push_back(cam);

    // add floor

    auto mat = new material{"floor"};
    mat->kd = {0.541176f, 0.709804f, 0.286275f};
    scen->materials.push_back(mat);
    auto shp = new shape{"floor"};
    shp->mat = mat;
    shp->pos = {{-100, 0, -100}, {100, 0, -100}, {100, 0, 100}, {-100, 0, 100}};
    shp->norm = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
    shp->texcoord = {{-10, -10}, {10, -10}, {10, 10}, {-10, 10}};
    shp->triangles = {{0, 1, 2}, {0, 2, 3}};
    scen->shapes.push_back(shp);
    scen->instances.push_back(new instance{"floor", {{1,0,0},{0,1,0},{0,0,1},{0,0.2,0}}, shp});

    auto graph = new Graph();

    build_graph_houses(scen,mapMat,graph);
    build_roads(scen,mapMat,graph);

    auto rng =  init_rng(0, static_cast<uint64_t>(time(NULL)));

    callBuild(scen,graph,graph->nodeStart,{{1,0,0},{0,1,0},{0,0,1},{-25,0,+25}},rng, dim,dim);
    /*
    graph.nodes.push_back(loadNode("Models/modularBuildings_010.obj", scen, mapMat));
    auto nod = loadNode("Models/modularBuildings_059.obj", scen, mapMat);
    auto nod1 = loadNode("Models/modularBuildings_060.obj", scen, mapMat);
    auto nod2 = loadNode("Models/modularBuildings_061.obj", scen, mapMat);
    add_node_to_scene(scen, &nod2, make_frame3_fromzx({0,0.6,0},{0,0,1},{1,0,0}));
     */


    save_scene(out,scen,save_options());

    delete graph;
    delete scen;
    delete mapMat;
}



