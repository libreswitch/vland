from opsvalidator.base import BaseValidator
from opsvalidator import error
from opsvalidator.error import ValidationError
from opsrest.utils.utils import get_column_data_from_row


class VlanValidator(BaseValidator):
    resource = "vlan"

    def validate_deletion(self, validation_args):
        vlan_row = validation_args.resource_row
        vlan_name = get_column_data_from_row(vlan_row, "name")
        if vlan_name == 'DEFAULT_VLAN_1':
            details = "DEFAULT_VLAN_1 cannot be deleted"
            raise ValidationError(error.VERIFICATION_FAILED, details)

    def validate_modification(self, validation_args):
        vlan_row = validation_args.resource_row
        vlan_id = get_column_data_from_row(vlan_row, "id")
        is_new = validation_args.is_new
        idl = validation_args.idl
        if is_new:
            vlan_id_list = []
            for ovs_rec in idl.tables["VLAN"].rows.itervalues():
                vlan_id_list.append(ovs_rec.id)
            if (len(vlan_id_list) != len(set(vlan_id_list))):
                details = "VLAN id %d already exists" % (vlan_id)
                raise ValidationError(error.VERIFICATION_FAILED, details)
