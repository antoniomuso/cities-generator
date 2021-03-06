//
// Created by antoniomusolino on 19/01/18.
//

/*
MIT License

Copyright (c) 2018 Antonio Musolino

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define YGL_OPENGL 1
#include "yocto-gl/yocto/yocto_gl.h"
using namespace ygl;

enum angles {
    _0 =0,
    _90 =90,
    _180 = 180,
    _270 = 270,
};

enum Collide {
    Left ,
    Right ,
    Forward ,
    Back ,
    LeftForward ,
    RightForward ,
    LeftBack ,
    RightBack ,
    NotCollide

};

struct Collision {
    Collide collide = NotCollide;
    int constant = 1;
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
    Collision move = {};
    vec3f constanValue = {0,0,0};
    angles rotation = _0;
    int maxDepth =-1;
    long inodeDep = -1;
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
        mater->rs = 0;
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


void add_node_to_graph(Graph* graph,node& nod) {
    if (nod.graphPos == -1) {
        nod.graphPos = graph->nodes.size();
        graph->nodes.push_back(nod);
    }
}

// Add rule " nod := .. | nod1 "
void add_once_node_or(node &nod, node &nod1, transform constValue, Graph *graph) {
    add_node_to_graph(graph,nod1);
    add_node_to_graph(graph,nod);

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
    add_node_to_graph(graph,nod);

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

    if(edge.transf.move.collide == NotCollide ) return false;

    if (rot == _0) {
        if (edge.transf.move.collide == Right || edge.transf.move.collide == RightForward || edge.transf.move.collide == RightBack) {
            matPos.x += edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Left || edge.transf.move.collide == LeftForward || edge.transf.move.collide == LeftBack) {
            matPos.x += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Forward || edge.transf.move.collide == RightForward || edge.transf.move.collide == LeftForward) {
            matPos.y += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Back || edge.transf.move.collide == LeftBack || edge.transf.move.collide == RightBack) {
            matPos.y += edge.transf.move.constant;
        }
    } else if (rot == _90) {
        if (edge.transf.move.collide == Right || edge.transf.move.collide == RightForward || edge.transf.move.collide == RightBack) {
            matPos.y += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Left || edge.transf.move.collide == LeftForward || edge.transf.move.collide == LeftBack) {
            matPos.y += +edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Forward || edge.transf.move.collide == RightForward || edge.transf.move.collide == LeftForward) {
            matPos.x += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Back || edge.transf.move.collide == LeftBack || edge.transf.move.collide == RightBack) {
            matPos.x += edge.transf.move.constant;
        }
    } else if (rot == _180) {
        if (edge.transf.move.collide == Right || edge.transf.move.collide == RightForward || edge.transf.move.collide == RightBack) {
            matPos.x += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Left || edge.transf.move.collide == LeftForward || edge.transf.move.collide == LeftBack) {
            matPos.x += edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Forward || edge.transf.move.collide == RightForward || edge.transf.move.collide == LeftForward) {
            matPos.y += edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Back || edge.transf.move.collide == LeftBack || edge.transf.move.collide == RightBack) {
            matPos.y += -edge.transf.move.constant;
        }
    } else {
        if (edge.transf.move.collide == Right || edge.transf.move.collide == RightForward || edge.transf.move.collide == RightBack) {
            matPos.y += edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Left || edge.transf.move.collide == LeftForward || edge.transf.move.collide == LeftBack) {
            matPos.y += -edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Forward || edge.transf.move.collide == RightForward || edge.transf.move.collide == LeftForward) {
            matPos.x += edge.transf.move.constant;
        }
        if (edge.transf.move.collide == Back || edge.transf.move.collide == LeftBack || edge.transf.move.collide == RightBack) {
            matPos.x += -edge.transf.move.constant;
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
void build (scene* scn, Graph* graph, long inode,frame3f pos,rng_pcg32& rng, position_matrix& blockPosition,angles rot, vec2i matPos, int depth, long inodeDep) {
    auto node = graph->nodes.at(inode);

    if (depth > 0) depth--;
    if (depth == 0) {
        if (inodeDep >= 0 )build(scn,graph,inodeDep,pos,rng, blockPosition,rot,matPos,-1, -1);
        return;
    }

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
        auto framTrasl = translation_frame3f(edge.transf.constanValue );
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

        build(scn, graph, edge.indexNode, newPos, rng, blockPosition,newAngle,posMat
                , edge.transf.maxDepth > 0 &&  (depth < 0 || edge.transf.maxDepth < depth) ? edge.transf.maxDepth : depth
                , edge.transf.inodeDep >= 0 ? edge.transf.inodeDep : inodeDep);
    }

}

position_matrix callBuild(scene* scn, Graph* graph, frame3f pos, unsigned int height, unsigned int width) {
    auto blockPosition = position_matrix(height,width);
    auto rng =  init_rng(0, static_cast<uint64_t>(time(NULL)));
    build(scn,graph,graph->nodeStart,pos,rng,blockPosition,_0, {int(height-1), int(width-1)}, -1, -1);
    return blockPosition;
}


// Create roads rules
void build_roads(scene* scen, std::map<string, material*>* mapMat, Graph* graph) {

    auto stradaConStrisciPedonale =  loadNode("myModels/roadTile_025.obj",scen,mapMat);
    auto stradaConPiccolaUscitaInBassoVerde = loadNode("myModels/roadTile_032.obj",scen,mapMat);
    auto stradaConUscitaGrandeInBassoVerde = loadNode("myModels/roadTile_150.obj",scen,mapMat);
    auto stradaDrittaBordiVerde = loadNode("myModels/roadTile_142.obj",scen,mapMat);
    //auto stradaDrittaSenzaUnBordoVerde= loadNode("ModelsRoads/roadTile_149.obj",scen,mapMat);
    //auto stradaDrittaVerdeRialzata= loadNode("ModelsRoads/roadTile_183.obj",scen,mapMat);
    auto incrocioAQuattroVerde = loadNode("myModels/roadTile_141.obj",scen,mapMat);
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
            {stradaConUscitaGrandeInBassoVerde, {}},
            //{incrocioAQuattroVerde, {NotCollide}}
    });
    add_multi_nodes_or(alberi, graph, {
            {albero, {}},
            {alberoBig,{{NotCollide},{0,0,0.125f}}}
    });

    // Node Obj
    add_multi_nodes_or(stradaConStrisciPedonale,graph,{
            {stradeDritte, {{Forward},{1,0,0}}},
            {stradeConCurve, {{Forward},{1,0,0},_90}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Left},{0,0.2,-1},_90}},
            {house,{{Right},{0,0.2,+1},_270}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {alberi,{{NotCollide},{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Left},{0,0.2,-1},_90}},
            {alberi,{{NotCollide},{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaConStrisciPedonale,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Right},{0,0.2,+1},_270}},
            //{alberi,{graph->unit.NotMove,{0.2,0.2,0.11f}}}
    });


    // Strada Normale
    add_multi_nodes_or(stradaDrittaVerde,graph,{
            {stradeDritte, {{Forward},{1,0,0}}},
            {stradeConCurve, {{Forward},{1,0,0},_90}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Left},{0,0.2,-1},_90}},
            {house,{{Right},{0,0.2,1},_270}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {alberi,{{NotCollide},{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Left},{0,0.2,-1},_90}},
            {alberi,{{NotCollide},{0.2,0.2,0.11f}}}
    });
    add_multi_nodes_and(stradaDrittaVerde,graph, {
            {stradeDritte,{{Forward},{1,0,0}}},
            {house,{{Right},{0,0.2,1},_270}}
    });



    add_multi_nodes_and(stradaConUscitaGrandeInBassoVerde,graph,{
            {stradeDritte,{{Forward},{1,0,0}}},
            {stradeDritte,{{Right},{0,0,1}, _270}}
    });
    add_multi_nodes_and(incrocioAQuattroVerde,graph,{
            {stradeDritte,{{Right},{0,0,1},_270}},
            {stradeDritte,{{Back},{-1,0,-0.086f}, _180}},
            {stradeDritte,{{Forward},{1,0,0}, _0}}
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

    // Blocchi Pallazzi Grandi.
    auto bloccoBianco = loadNode("Models/modularBuildings_018.obj", scen, mapMat);
    auto bloccoBuild = loadNode("Models/modularBuildings_018.obj", scen, mapMat);
    auto bloccoBiancoOnly = loadNode("Models/modularBuildings_018.obj", scen, mapMat);
    auto bloccoPiattoSpessore = loadNode("Models/modularBuildings_019.obj", scen, mapMat);
    auto bloccoConSolaFinestraSinistra = loadNode("Models/modularBuildings_062.obj", scen, mapMat);
    auto bloccoConSolaFinestraDestra = loadNode("Models/modularBuildings_067.obj", scen, mapMat);

    //Abbellimenti palazzi
    auto BloccoColorato = loadNode("Models/modularBuildings_010.obj", scen, mapMat);

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
    auto tetto5 = loadNode("Models/modularBuildings_011.obj", scen, mapMat);


    //finestre
    //auto portaAdArco = loadNode("Models/modularBuildings_099.obj", scen, mapMat);



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
            {tettoConFinestra, {{},{1, 0, 0}}},
            {tettoTriangolo, {}},
            {tetto2,{{},{1,0,0}}},
            {tetto3,{{},{1,0,0}}},
            {tetto4,{{},{1,0,0}}},
            {tetto5,{{}}}
    });
    add_multi_nodes_or(piani, graph, {
            {pianoFinestre,  {}},
            {pianoFinestrone, {}},
            {pianoFinestreQuadrate, {{}}},
            {pianoConBalcone,{}},
            {pianoConFinestreCoperte,{}}
    });

    //Metto che la variabile di start parte con una base
    add_multi_nodes_or(startNode, graph, {
            {basi,{}}
    });


    //add_multi_nodes(n, graph, { {{0,0,0},nr1},{{0,0,0},nr2} } );


    //Varaibile BaseConScalinata
    add_multi_nodes_or(BaseConScalinata, graph, {
            {piani,  {{},{0, 0.8, 0}}},
            {BloccoColorato, {{},{0, 0.8, 0}}},
            {tetti, {{},{0, 0.8, 0}}}
    });

    add_multi_nodes_or(BaseConScalinataEFinestre, graph, {
            {piani,  {{},{0, 0.8, 0}}},
            {BloccoColorato, {{},{0, 0.8, 0}}},
            {tetti, {{},{0, 0.8, 0}}}
    });


    //Variabili piani
    add_multi_nodes_or(pianoFinestre, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {piani,  {{},{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoFinestrone, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {piani,  {{},{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoFinestreQuadrate, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {piani,  {{},{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoConBalcone, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {piani,  {{},{0, 0.6, 0}}}
    });

    add_multi_nodes_or(pianoConFinestreCoperte, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {piani,  {{},{0, 0.6, 0}}}
    });

    // Variabile Base con finestre
    add_multi_nodes_or(baseConFinestreEPortone, graph, {
            {tetti, {{},{0, 0.6, 0}}},
            {BloccoColorato, {{},{0, 0.6, 0}}},
            {piani, {{},{0, 0.6, 0}}},
            {piani, {{},{0, 0.6, 0}}}
    });

    add_multi_nodes_or(BloccoColorato,graph, {
            {piani, {{},{0, 0.2, 0}}}
    });



    // blocchi forma figa
    auto bloccoQuadratoPentagono = loadNode("Models/modularBuildings_037.obj", scen, mapMat);
    //auto bloccoPenatagonoQuadrato = loadNode("Models/modularBuildings_036.obj", scen, mapMat);
    auto bloccoQuadratoPentagonoFinestra = loadNode("Models/modularBuildings_062.obj", scen, mapMat);
    auto bloccoPentagonoFinestra = loadNode("Models/modularBuildings_039.obj", scen, mapMat);
    auto bloccoPentagono = loadNode("Models/modularBuildings_056.obj", scen, mapMat);
    auto bloccoPentagonoFinestraLungaMezzo = loadNode("Models/modularBuildings_049.obj", scen, mapMat);
    auto bloccoBiancoPent = loadNode("Models/modularBuildings_018.obj", scen, mapMat);
    auto tettoPentagonoPent = loadNode("Models/modularBuildings_006.obj", scen, mapMat);
    add_node_to_graph(graph,tettoPentagonoPent);

    add_multi_nodes_and(baseConFinestreEPortone,graph, {
            {bloccoBiancoPent, {{Left},{0,0,-1},_270,10}},
            {bloccoBiancoPent, {{LeftForward},{1,0,-1},_180,10}},
            {bloccoBiancoPent, {{Forward},{1,0,0},_90,10}},
            {bloccoQuadratoPentagono,{{}, {0,0.6,0},_0,9}}
    });

    add_multi_nodes_and(baseConFinestreEPortone,graph, {
            {bloccoQuadratoPentagonoFinestra, {{Left},{0,0,-1},_270,10}},
            {bloccoQuadratoPentagonoFinestra, {{LeftForward},{1,0,-1},_180,10}},
            {bloccoBiancoPent, {{Forward},{1,0,0},_90,10}},
            {bloccoQuadratoPentagono,{{}, {0,0.6,0},_0,9}}
    });

    add_multi_nodes_or(bloccoQuadratoPentagonoFinestra,graph, {
            {bloccoQuadratoPentagono,{{},{0,0.6,0}}}
    });


    add_multi_nodes_or(bloccoBiancoPent,graph, {
            {bloccoQuadratoPentagono,{{},{0,0.6,0}}}
    });

    add_multi_nodes_or(bloccoQuadratoPentagono,graph,{
            {bloccoPentagonoFinestra,{{},{0,0.6,0},_0,-1,tettoPentagonoPent.graphPos}}
    });

    add_multi_nodes_or(bloccoPentagonoFinestra,graph, {
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            {bloccoPentagono,{{},{0,0.6,0}}},
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            //{bloccoPenatagonoQuadrato,{{},{0,0.6,0}}}
    });

    add_multi_nodes_or(bloccoPentagono,graph, {
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            {bloccoPentagono,{{},{0,0.6,0}}},
            {bloccoPentagonoFinestra,{{},{0,0.6,0}}},
            //{bloccoPenatagonoQuadrato,{{},{0,0.6,0}}}
    });
    /*

    add_multi_nodes_or(bloccoPenatagonoQuadrato,graph, {
            {piani,{{}}}
    });
     */




    auto pianoFinestreQuadratePalazzi = loadNode("myModels/modularBuildings_034.obj", scen, mapMat);
    auto cornicioneTettoAngolo = loadNode("myModels/modularBuildings_0082.obj", scen, mapMat);
    auto cornicioneTettoLato = loadNode("myModels/modularBuildings_009.obj", scen, mapMat);
    auto senzaCornicione = loadNode("Models/modularBuildings_016.obj", scen, mapMat);


    auto nodeCornicioneLato = node{};
    auto nodeCornicioneLatoBack = node{};

    auto nodeCornicioneAngoloFSinistra = node{};
    auto nodeCornicioneAngoloFDestra = node{};
    auto nodeCornicioneAngoloDestra = node{};
    auto nodeCornicioneAngoloSinistra = node{};

    auto pianoPalazzo = node{};

    add_multi_nodes_or(nodeCornicioneLato,graph ,{
            {cornicioneTettoLato,{{},{},_90}}
    });

    add_multi_nodes_or(nodeCornicioneLatoBack,graph ,{
            {cornicioneTettoLato,{{},{},_90}}
    });

    add_multi_nodes_or(nodeCornicioneAngoloSinistra,graph ,{
            {cornicioneTettoAngolo,{{NotCollide},{},_0}}
    });

    add_multi_nodes_or(nodeCornicioneAngoloDestra,graph ,{
            {cornicioneTettoAngolo,{{NotCollide},{},_90}}
    });

    add_multi_nodes_or(nodeCornicioneAngoloFSinistra,graph ,{
            {cornicioneTettoAngolo,{{},{},_90}}
    });

    add_multi_nodes_or(nodeCornicioneAngoloFDestra,graph ,{
            {cornicioneTettoAngolo,{{},{}}}
    });

    add_multi_nodes_and(baseConFinestreEPortone,graph, {
            {bloccoBiancoOnly, {{Left},{0,0,-1},_0,10,nodeCornicioneAngoloSinistra.graphPos}},
            {bloccoBiancoOnly, {{Right},{0,0,1},_0,10,nodeCornicioneAngoloDestra.graphPos}},
            {bloccoBiancoOnly, {{LeftForward},{1,0,-1},_180,10,nodeCornicioneAngoloFSinistra.graphPos}},
            {bloccoBiancoOnly, {{RightForward},{1,0,1},_180,10,nodeCornicioneAngoloFDestra.graphPos}},
            {bloccoBiancoOnly, {{Forward},{1,0,0},_180,10,nodeCornicioneLato.graphPos}},
            {pianoPalazzo,{{}, {0,0.6,0},_0,9,nodeCornicioneLato.graphPos}}
    });


    add_multi_nodes_and(baseConFinestreEPortone,graph, {
            {bloccoBiancoOnly, {{Left},{0,0,-1},_0,5,nodeCornicioneAngoloSinistra.graphPos}},
            {bloccoBiancoOnly, {{Right},{0,0,1},_0,5,nodeCornicioneAngoloDestra.graphPos}},
            {bloccoBiancoOnly, {{LeftForward},{1,0,-1},_180,5,nodeCornicioneAngoloFSinistra.graphPos}},
            {bloccoBiancoOnly, {{RightForward},{1,0,1},_180,5,nodeCornicioneAngoloFDestra.graphPos}},
            {bloccoBiancoOnly, {{Forward},{1,0,0},_180,5,nodeCornicioneLato.graphPos}},
            {pianoPalazzo,{{}, {0,0.6,0},_0,4,nodeCornicioneLato.graphPos}}
    });

    add_multi_nodes_or(pianoPalazzo, graph, {
            {bloccoConSolaFinestraSinistra,{}},
            {bloccoConSolaFinestraDestra,{}},
            {pianoFinestreQuadratePalazzi,{}},
            {pianoFinestreQuadratePalazzi,{}},
            {pianoFinestreQuadratePalazzi,{}},
            {bloccoBiancoOnly,{}}
    });

    add_multi_nodes_or(bloccoConSolaFinestraDestra, graph, {
            {pianoPalazzo,{{},{0,0.6,0}}},
    });

    add_multi_nodes_or(bloccoConSolaFinestraSinistra, graph, {
            {pianoPalazzo,{{},{0,0.6,0}}},
    });

    add_multi_nodes_and(bloccoBiancoOnly,graph, {
            {pianoPalazzo, {{}, {0,0.6,0}}}
    });

    add_multi_nodes_and(pianoFinestreQuadratePalazzi,graph, {
            {pianoPalazzo, {{}, {0,0.6,0}}}
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
        auto light = 30.0f;
        lmat->ke = {130 * light, 94 * light, 90 * light};
        lmat->kd = {1.0f, 0.57647058823f, 0.16078431372f};
        lshp->mat = lmat;
        lshp->mat->rs =0;

        scen->shapes.push_back(lshp);
        scen->materials.push_back(lmat);
        scen->instances.push_back(
                new instance{"light", {{1,0,0},{0,1,0},{0,0,1},{0,15,50}}, lshp});
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
    mat->rs = 0;
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


    callBuild(scen,graph,{{1,0,0},{0,1,0},{0,0,1},{-25,0,+25}}, dim,dim);
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



