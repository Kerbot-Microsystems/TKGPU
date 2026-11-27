/* Metadata fields are independent: setter never runs the delete callback. */
int Tcl_GetCommandInfoFromToken(Tcl_Command token,Tcl_CmdInfo *info) {
    if(!token||!info||token->retired) return 0;
    *info=(Tcl_CmdInfo){
        .isNativeObjectProc=token->proc!=command_info_object,
        .objProc=token->proc,.objClientData=token->obj_data,
        .proc=token->string_proc,.clientData=token->string_data,
        .deleteProc=token->dispose,.deleteData=token->delete_data,
        .namespacePtr=token->namespace
    };
    return 1;
}
int Tcl_SetCommandInfoFromToken(Tcl_Command token,const Tcl_CmdInfo *info) {
    if(!token||!info||token->retired) return 0;
    token->proc=info->objProc?info->objProc:command_info_object;
    token->obj_data=info->objProc?info->objClientData:token;
    token->string_proc=info->proc;
    token->string_data=info->clientData;
    token->dispose=info->deleteProc;
    token->delete_data=info->deleteData;
    return 1;
}
