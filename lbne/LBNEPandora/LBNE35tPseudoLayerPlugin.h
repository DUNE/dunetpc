/**
 *  @file   lbnecode/LBNEPandora/LBNE35tPseudoLayerPlugin.h
 *
 *  @brief  Header file for the LBNE35t pseudo layer plugin class.
 *
 *  $Log: $
 */
#ifndef LBNE_35T_PSEUDO_LAYER_PLUGIN_H
#define LBNE_35T_PSEUDO_LAYER_PLUGIN_H 1

#include "LArPandoraAlgorithms/LArPlugins/LArPseudoLayerPlugin.h"

namespace lar_pandora
{

/**
 *  @brief  LBNE35tPseudoLayerPlugin class
 */
class LBNE35tPseudoLayerPlugin : public lar_content::LArPseudoLayerPlugin
{
public:
    /**
     *  @brief  Default constructor
     */
    LBNE35tPseudoLayerPlugin();
};

} // namespace lar

#endif // #ifndef LBNE_35T_PSEUDO_LAYER_PLUGIN_H
