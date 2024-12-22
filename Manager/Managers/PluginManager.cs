﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;

using Manager.Classes;

using IVSDKDotNet;
using IVSDKDotNet.Manager;

namespace Manager.Managers
{
    public class PluginManager
    {

        #region Variables
        // Lists
        public List<FoundPlugin> ActivePlugins;
        
        // Other
        private readonly object activePluginsLockObj = new object();
        public DateTime TimeSinceLastPluginLoad;
        #endregion

        #region Raisers
        public void RaiseTick()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseTick();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "Tick", ex);
                }
            });
        }
        public void RaiseGameLoad()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseGameLoad();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "GameLoad", ex);
                }
            });
        }
        public void RaiseGameLoadPriority()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseGameLoadPriority();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "GameLoadPriority", ex);
                }
            });
        }
        public void RaiseMountDevice()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseMountDevice();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "MountDevice", ex);
                }
            });
        }

        public void RaiseOnImGuiGlobalRenderingEvent(IntPtr devicePtr, ImGuiIV_DrawingContext ctx)
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnImGuiGlobalRendering(devicePtr, ctx);
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "OnImGuiGlobalRendering", ex);
                }

                // The plugin itself might've changed the style of ImGui so we need to reset it for the next plugin
                Main.Instance.ResetImGuiStyle();
            });
        }
        public void RaiseOnImGuiManagerRenderingEvent(FoundPlugin plugin, IntPtr devicePtr)
        {
            if (plugin == null)
                return;

            try
            {
                plugin.RaiseOnImGuiManagerRendering(devicePtr);
            }
            catch (Exception ex)
            {
                HandlePluginException(plugin, 6d, "OnImGuiManagerRendering", ex);
            }
        }

        public void RaiseOnShutdown()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnShutdown();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "OnShutdown", ex);
                }
            });
        }

        public void RaiseOnScriptAbort(Guid scriptID)
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnScriptAbort(scriptID);
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "OnScriptAbort", ex);
                }
            });
        }
        public void RaiseOnScriptLoad(Guid scriptID)
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnScriptLoad(scriptID);
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "OnScriptLoad", ex);
                }
            });
        }

        public void RaiseOnBeforeScriptsAbort(Guid[] scriptIDs)
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnBeforeScriptsAbort(scriptIDs);
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "RaiseOnBeforeScriptsAbort", ex);
                }
            });
        }
        public void RaiseOnAfterScriptsAbort()
        {
            ActivePlugins.ForEach(x =>
            {
                try
                {
                    x.RaiseOnAfterScriptsAbort();
                }
                catch (Exception ex)
                {
                    HandlePluginException(x, 6d, "RaiseOnAfterScriptsAbort", ex);
                }
            });
        }
        #endregion

        #region Methods

        // Load stuff
        public void LoadPlugins()
        {
            if (!Directory.Exists(CLR.CLRBridge.IVSDKDotNetPluginsPath))
            {
                Directory.CreateDirectory(CLR.CLRBridge.IVSDKDotNetPluginsPath);
                return;
            }

            // Unload currently loaded IV-SDK .NET Manager plugins
            UnloadPlugins(AbortReason.Manager, false);

            // Load IV-SDK .NET Manager plugins
            string[] pluginFiles = Directory.GetFiles(CLR.CLRBridge.IVSDKDotNetPluginsPath, "*.plug.dll", SearchOption.TopDirectoryOnly).OrderBy(x => Path.GetFileName(x)).ToArray();

            for (int i = 0; i < pluginFiles.Length; i++)
                LoadAssembly(pluginFiles[i]);

            // Log
            Logger.Log(string.Format("Finished loading {0} IV-SDK .NET Manager Plugin(s).", ActivePlugins.Count));

            TimeSinceLastPluginLoad = DateTime.Now;
        }

        public bool LoadAssembly(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
                return false;

            // Get file name without extension from given path
            string fileName = Path.GetFileNameWithoutExtension(path).Replace(".plug", "");

            // Check if plugin is already loaded
            FoundPlugin foundPlugin = GetFoundPlugin(fileName);

            if (foundPlugin != null)
            {
                Logger.LogWarning(string.Format("Not loading IV-SDK .NET Manager Plugin '{0}' because it is already loaded.", fileName));
                return false;
            }

            // Try loading the plugin
            try
            {
                using (FileStream fs = new FileStream(path, FileMode.Open, FileAccess.Read))
                {
                    // Get byte array from file stream
                    byte[] fileByteArray = Helper.GetByteArray(fs);

                    // Load the assembly into the domain
                    Assembly assembly = Assembly.Load(fileByteArray);

                    // Get the first class that inherites from the ManagerPlugin class
                    Type scriptType = assembly.GetTypes().Where(x => x.IsSubclassOf(typeof(ManagerPlugin))).FirstOrDefault();

                    // Could not find any classes that inherit the ManagerPlugin class
                    if (scriptType == null)
                    {
                        Logger.LogWarning(string.Format("Could not load IV-SDK .NET Manager Plugin '{0}' because the entry-point for the Plugin could not be found.", fileName));
                        return false;
                    }

                    // Log
                    Logger.Log(string.Format("Found IV-SDK .NET Plugin: {0}", scriptType.FullName));

                    // Create new instance of type for assembly
                    ManagerPlugin plugin = (ManagerPlugin)assembly.CreateInstance(scriptType.FullName);

                    if (plugin == null)
                    {
                        Logger.LogWarning(string.Format("An unknown error occured while trying to create new instance of IV-SDK .NET Manager Plugin '{0}'.", fileName));
                        return false;
                    }

                    // Check for plugin resource folder
                    string resourceFolderPath = string.Format("{0}\\{1}", CLR.CLRBridge.IVSDKDotNetPluginsPath, fileName);
                    if (Directory.Exists(resourceFolderPath))
                        plugin.PluginResourceFolder = resourceFolderPath;

                    // Register AssemblyResolve event
                    //script.ScriptDomain.AssemblyResolve += ScriptDomain_AssemblyResolve;

                    // Add plugin to activePlugins list.
                    ActivePlugins.Add(new FoundPlugin(fileName, path, assembly, plugin, scriptType));

                    return true;
                }
            }
            catch (ReflectionTypeLoadException ex)
            {
                Exception[] exs = ex.LoaderExceptions;
                for (int i = 0; i < exs.Length; i++)
                {
                    Exception e = exs[i];
                    Logger.LogError(string.Format("A ReflectionTypeLoadException occured while trying to load IV-SDK .NET Manager Plugin '{0}'. Details: {1}", fileName, e));
                }

#if DEBUG
                // Throw on exception when manager is a debug build and a debugger is attached
                if (Debugger.IsAttached)
                    throw;
#endif
            }
            catch (Exception ex)
            {
                Logger.LogError(string.Format("An exception occured while trying to load IV-SDK .NET Manager Plugin '{0}'. Details: {1}", fileName, ex));

#if DEBUG
                // Throw on exception when manager is a debug build and a debugger is attached
                if (Debugger.IsAttached)
                    throw;
#endif
            }

            return false;
        }

        // Unload stuff
        public void UnloadPlugins(AbortReason reason, bool showMessage)
        {
            if (ActivePlugins.Count == 0)
            {
                if (showMessage)
                    Logger.Log("There are no plugins to unload.");

                return;
            }

            // Unload plugins
            FoundPlugin[] arr = ActivePlugins.Where(x => !x.ThePluginInstance.ForceNoAbort).ToArray();

            for (int i = 0; i < arr.Length; i++)
                arr[i].Abort(reason, showMessage);

            ActivePlugins.RemoveAll(x =>
            {
                // Dont remove plugin from list if it cannot be aborted
                if (x.ThePluginInstance.ForceNoAbort)
                    return false;

                return true;
            });

            // Unload plugins
            //ActivePlugins.ForEach(x => x.Abort(reason, showMessage));
            //ActivePlugins.Clear();

            // Force garbage collection
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }

        // Other
        public void HandlePluginException(FoundPlugin target, double notifySecondsVisible, string eventErrorOccuredIn, Exception ex, bool isInternalEvent = false)
        {
            // Stop plugin so it doesn't raise any events anymore
            target.Stop();

            string pluginName = target.EntryPoint.FullName;

            // Show and Log the error
            Main.Instance.Notification.ShowNotification(
                NotificationType.Error,
                DateTime.UtcNow.AddSeconds(notifySecondsVisible),
                string.Format("An error occured in IV-SDK .NET Manager Plugin {0} {1}.", pluginName, eventErrorOccuredIn),
                ex.Message,
                string.Format("ERROR_IN_PLUGIN_{0}", pluginName));

            Logger.LogError(string.Format("An error occured while processing '{0}' event for IV-SDK .NET Manager Plugin '{1}'. Unloading Plugin. Details: {2}", eventErrorOccuredIn, pluginName, ex));

            // Unload plugin
            UnloadPlugin(AbortReason.Manager, target, true);
        }

        #endregion

        #region Functions
        public FoundPlugin GetFoundPlugin(string name)
        {
            if (string.IsNullOrWhiteSpace(name))
                return null;

            FoundPlugin fp = null;

            lock (activePluginsLockObj)
            {
                fp = ActivePlugins.Where(x => x.FileName.ToLower() == name.ToLower()
                    || x.EntryPoint.FullName.ToLower() == name.ToLower()).FirstOrDefault();
            }

            return fp;
        }
        public FoundPlugin GetFoundPlugin(Guid id)
        {
            FoundPlugin fp = null;

            lock (activePluginsLockObj)
            {
                fp = ActivePlugins.Where(x => x.ID == id).FirstOrDefault();
            }

            return fp;
        }

        public bool UnloadPlugin(AbortReason reason, FoundPlugin plugin, bool showMessage)
        {
            if (plugin != null)
            {
                if (showMessage)
                    Logger.Log(string.Format("Unloading Plugin {0}...", plugin.EntryPoint.FullName));

                plugin.Abort(reason, showMessage);

                bool result = ActivePlugins.Remove(plugin);
                GC.Collect();
                return result;
            }

            return false;
        }
        #endregion

        #region Constructor
        public PluginManager()
        {
            ActivePlugins = new List<FoundPlugin>();
        }
        #endregion

    }
}