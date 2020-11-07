// SPDX-License-Identifier: Apache-2.0
// Copyright 2017 Loopring Technology Limited.
pragma solidity ^0.7.0;

import "../base/Controller.sol";
import "../iface/ModuleRegistry.sol";
import "../iface/Version.sol";
import "../iface/Module2.sol";

/// @title Wallet
/// @dev Base contract for smart wallets.
///      Sub-contracts must NOT use non-default constructor to initialize
///      wallet states, instead, `init` shall be used. This is to enable
///      proxies to be deployed in front of the real wallet contract for
///      saving gas.
///
/// @author Daniel Wang - <daniel@loopring.org>
abstract contract BaseVersion is Version
{
    event ModuleAdded (address module, bool isFeatureModule);

    string    public override label;
    address[] public initModules;
    address[] public featureModules;
    mapping (address => bool)    public featureModuleMap;
    mapping (bytes4  => address) public methodToModule;

    constructor(
        Controller       _controller,
        string    memory _label,
        address[] memory _initModules,
        address[] memory _featureModules
        )
    {
        label = _label;

        ModuleRegistry moduleRegistry = _controller.moduleRegistry();

        for (uint i = 0; i < _initModules.length; i++) {
            _addInitModule(moduleRegistry, _initModules[i]);
        }

        for (uint i = 0; i < _featureModules.length; i++) {
            _addFeatureModule(moduleRegistry, _featureModules[i]);
        }
    }

    function activate()
        external
        override
    {
        for (uint i = 0; i < initModules.length; i++) {
            Module2(initModules[i]).activate(msg.sender);
        }

        for (uint i = 0; i < featureModules.length; i++) {
            Module2(featureModules[i]).activate(msg.sender);
        }
    }

    function deactivate()  external override pure {}

    /// @dev Checks if a module has been added to this wallet.
    /// @param _module The module to check.
    /// @return True if the module exists; False otherwise.
    function hasModule(address _module)
        public
        override
        view
        returns (bool)
    {
        return featureModuleMap[_module];
    }

    function boundMethodModule(bytes4 _method)
        public
        override
        view
        returns (address _module)
    {
        return methodToModule[_method];
    }

    // ----- internal -----

    function _addInitModule(ModuleRegistry moduleRegistry, address _module)
        private
    {
        require(_module != address(0), "NULL_MODULE");
        require(moduleRegistry.isModuleEnabled(_module), "INVALID_MODULE");

        initModules.push(_module);
        emit ModuleAdded(_module, false);
    }

    function _addFeatureModule(ModuleRegistry moduleRegistry, address _module)
        private
    {
        require(_module != address(0), "NULL_MODULE");
        require(moduleRegistry.isModuleEnabled(_module), "INVALID_MODULE");

        Module2 m = Module2(_module);
        bytes4[] memory methods = m.bindableMethods();
        for (uint i = 0; i < methods.length; i++) {
            require(methods[i] != bytes4(0), "BAD_METHOD");
            methodToModule[methods[i]] = _module;
        }

        featureModules.push(_module);
        featureModuleMap[_module] = true;
        emit ModuleAdded(_module, true);
    }
}
